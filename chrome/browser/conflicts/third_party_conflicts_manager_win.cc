// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/third_party_conflicts_manager_win.h"

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/conflicts/incompatible_applications_updater_win.h"
#include "chrome/browser/conflicts/installed_applications_win.h"
#include "chrome/browser/conflicts/module_blacklist_cache_updater_win.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/browser/conflicts/module_info_util_win.h"
#include "chrome/browser/conflicts/module_list_filter_win.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

std::unique_ptr<CertificateInfo> CreateExeCertificateInfo() {
  auto certificate_info = std::make_unique<CertificateInfo>();

  base::FilePath exe_path;
  if (base::PathService::Get(base::FILE_EXE, &exe_path))
    GetCertificateInfo(exe_path, certificate_info.get());

  return certificate_info;
}

std::unique_ptr<ModuleListFilter> CreateModuleListFilter(
    const base::FilePath& module_list_path) {
  auto module_list_filter = std::make_unique<ModuleListFilter>();

  if (!module_list_filter->Initialize(module_list_path))
    return nullptr;

  return module_list_filter;
}

}  // namespace

ThirdPartyConflictsManager::ThirdPartyConflictsManager(
    ModuleDatabase* module_database)
    : module_database_(module_database),
      background_sequence_(base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
           base::MayBlock()})),
      module_list_received_(false),
      on_module_database_idle_called_(false),
      weak_ptr_factory_(this) {
  base::PostTaskAndReplyWithResult(
      background_sequence_.get(), FROM_HERE,
      base::BindOnce(&CreateExeCertificateInfo),
      base::BindOnce(&ThirdPartyConflictsManager::OnExeCertificateCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

ThirdPartyConflictsManager::~ThirdPartyConflictsManager() = default;

// static
void ThirdPartyConflictsManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  // Register the pref that remembers the MD5 digest for the current module
  // blacklist cache. The default value is an invalid MD5 digest.
  registry->RegisterStringPref(prefs::kModuleBlacklistCacheMD5Digest, "");
}

// static
void ThirdPartyConflictsManager::DisableThirdPartyModuleBlocking(
    base::TaskRunner* background_sequence) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Delete the module blacklist cache. Since the NtMapViewOfSection hook only
  // blocks if the file is present, this will deactivate third-party modules
  // blocking for the next browser launch.
  background_sequence->PostTask(
      FROM_HERE,
      base::BindOnce(&ModuleBlacklistCacheUpdater::DeleteModuleBlacklistCache));

  // Also clear the MD5 digest since there will no longer be a current module
  // blacklist cache.
  g_browser_process->local_state()->ClearPref(
      prefs::kModuleBlacklistCacheMD5Digest);
}

// static
void ThirdPartyConflictsManager::ShutdownAndDestroy(
    std::unique_ptr<ThirdPartyConflictsManager> instance) {
  DisableThirdPartyModuleBlocking(instance->background_sequence_.get());
  // |instance| is intentionally destroyed at the end of the function scope.
}

void ThirdPartyConflictsManager::OnModuleDatabaseIdle() {
  if (on_module_database_idle_called_)
    return;

  on_module_database_idle_called_ = true;

  base::PostTaskAndReplyWithResult(
      background_sequence_.get(), FROM_HERE, base::BindOnce([]() {
        return std::make_unique<InstalledApplications>();
      }),
      base::BindOnce(
          &ThirdPartyConflictsManager::OnInstalledApplicationsCreated,
          weak_ptr_factory_.GetWeakPtr()));
}

void ThirdPartyConflictsManager::LoadModuleList(const base::FilePath& path) {
  if (module_list_received_)
    return;

  module_list_received_ = true;

  base::PostTaskAndReplyWithResult(
      background_sequence_.get(), FROM_HERE,
      base::BindOnce(&CreateModuleListFilter, path),
      base::BindOnce(&ThirdPartyConflictsManager::OnModuleListFilterCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ThirdPartyConflictsManager::OnExeCertificateCreated(
    std::unique_ptr<CertificateInfo> exe_certificate_info) {
  exe_certificate_info_ = std::move(exe_certificate_info);

  if (module_list_filter_ && installed_applications_)
    InitializeIncompatibleApplicationsUpdater();

  if (module_list_filter_)
    MaybeInitializeModuleBlacklistCacheUpdater();
}

void ThirdPartyConflictsManager::OnModuleListFilterCreated(
    std::unique_ptr<ModuleListFilter> module_list_filter) {
  module_list_filter_ = std::move(module_list_filter);

  // A valid |module_list_filter_| is critical to the blocking of third-party
  // modules. By returning early here, the |incompatible_applications_updater_|
  // instance never gets created, thus disabling the identification of
  // incompatible applications.
  if (!module_list_filter_) {
    // Mark the module list as not received so that a new one may trigger the
    // creation of a valid filter.
    module_list_received_ = false;
    return;
  }

  if (exe_certificate_info_ && installed_applications_)
    InitializeIncompatibleApplicationsUpdater();

  if (exe_certificate_info_)
    MaybeInitializeModuleBlacklistCacheUpdater();
}

void ThirdPartyConflictsManager::OnInstalledApplicationsCreated(
    std::unique_ptr<InstalledApplications> installed_applications) {
  installed_applications_ = std::move(installed_applications);

  if (exe_certificate_info_ && module_list_filter_)
    InitializeIncompatibleApplicationsUpdater();
}

void ThirdPartyConflictsManager::InitializeIncompatibleApplicationsUpdater() {
  DCHECK(exe_certificate_info_);
  DCHECK(module_list_filter_);
  DCHECK(installed_applications_);

  incompatible_applications_updater_ =
      std::make_unique<IncompatibleApplicationsUpdater>(
          *exe_certificate_info_, *module_list_filter_,
          *installed_applications_);
  module_database_->AddObserver(incompatible_applications_updater_.get());
}

void ThirdPartyConflictsManager::MaybeInitializeModuleBlacklistCacheUpdater() {
  DCHECK(exe_certificate_info_);
  DCHECK(module_list_filter_);

  if (!base::FeatureList::IsEnabled(features::kThirdPartyModulesBlocking))
    return;

  // Create the instance. It is safe to use base::Unretained() since the
  // callback is not invoked when the updater is freed.
  module_blacklist_cache_updater_ =
      std::make_unique<ModuleBlacklistCacheUpdater>(
          *exe_certificate_info_, *module_list_filter_,
          base::BindRepeating(
              &ThirdPartyConflictsManager::OnModuleBlacklistCacheUpdated,
              base::Unretained(this)));
  module_database_->AddObserver(module_blacklist_cache_updater_.get());
}

void ThirdPartyConflictsManager::OnModuleBlacklistCacheUpdated(
    const ModuleBlacklistCacheUpdater::CacheUpdateResult& result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check that the MD5 digest of the old cache matches what was expected. Only
  // used for reporting a metric.
  const PrefService::Preference* preference =
      g_browser_process->local_state()->FindPreference(
          prefs::kModuleBlacklistCacheMD5Digest);
  DCHECK(preference);

  // The first time this is executed, the pref doesn't yet hold a valid MD5
  // digest.
  if (!preference->IsDefaultValue()) {
    const std::string old_md5_string =
        base::MD5DigestToBase16(result.old_md5_digest);
    const std::string& current_md5_string = preference->GetValue()->GetString();
    UMA_HISTOGRAM_BOOLEAN("ModuleBlacklistCache.ExpectedMD5Digest",
                          old_md5_string == current_md5_string);
  }

  // Set the expected MD5 digest for the next time the cache is updated.
  g_browser_process->local_state()->Set(
      prefs::kModuleBlacklistCacheMD5Digest,
      base::Value(base::MD5DigestToBase16(result.new_md5_digest)));
}
