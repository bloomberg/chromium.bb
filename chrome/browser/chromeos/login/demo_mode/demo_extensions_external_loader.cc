// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_extensions_external_loader.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/external_provider_impl.h"

namespace chromeos {

namespace {

// Arbitrary, but reasonable size limit in bytes for prefs file.
constexpr size_t kPrefsSizeLimit = 1024 * 1024;

base::Optional<base::Value> LoadPrefsFromDisk(
    const base::FilePath& prefs_path) {
  if (!base::PathExists(prefs_path)) {
    LOG(WARNING) << "Demo extensions prefs not found " << prefs_path.value();
    return base::nullopt;
  }

  std::string prefs_str;
  if (!base::ReadFileToStringWithMaxSize(prefs_path, &prefs_str,
                                         kPrefsSizeLimit)) {
    LOG(ERROR) << "Failed to read prefs " << prefs_path.value() << "; "
               << "failed after reading " << prefs_str.size() << " bytes";
    return base::nullopt;
  }

  std::unique_ptr<base::Value> prefs_value = base::JSONReader::Read(prefs_str);
  if (!prefs_value) {
    LOG(ERROR) << "Unable to parse demo extensions prefs.";
    return base::nullopt;
  }

  if (!prefs_value->is_dict()) {
    LOG(ERROR) << "Demo extensions prefs not a dictionary.";
    return base::nullopt;
  }

  return base::Value::FromUniquePtrValue(std::move(prefs_value));
}

}  // namespace

// static
bool DemoExtensionsExternalLoader::SupportedForProfile(Profile* profile) {
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile))
    return false;

  DemoSession* demo_session = DemoSession::Get();
  return demo_session && demo_session->started();
}

DemoExtensionsExternalLoader::DemoExtensionsExternalLoader()
    : weak_ptr_factory_(this) {
  DCHECK(DemoSession::Get() && DemoSession::Get()->started());
}

DemoExtensionsExternalLoader::~DemoExtensionsExternalLoader() = default;

void DemoExtensionsExternalLoader::StartLoading() {
  DemoSession::Get()->EnsureOfflineResourcesLoaded(base::BindOnce(
      &DemoExtensionsExternalLoader::StartLoadingFromOfflineDemoResources,
      weak_ptr_factory_.GetWeakPtr()));
}

void DemoExtensionsExternalLoader::StartLoadingFromOfflineDemoResources() {
  DemoSession* demo_session = DemoSession::Get();
  DCHECK(demo_session->offline_resources_loaded());

  base::FilePath demo_extension_list =
      demo_session->GetExternalExtensionsPrefsPath();
  if (demo_extension_list.empty()) {
    LoadFinished(std::make_unique<base::DictionaryValue>());
    return;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&LoadPrefsFromDisk, demo_extension_list),
      base::BindOnce(
          &DemoExtensionsExternalLoader::DemoExternalExtensionsPrefsLoaded,
          weak_ptr_factory_.GetWeakPtr()));
}

void DemoExtensionsExternalLoader::DemoExternalExtensionsPrefsLoaded(
    base::Optional<base::Value> prefs) {
  if (!prefs.has_value()) {
    LoadFinished(std::make_unique<base::DictionaryValue>());
    return;
  }
  DCHECK(prefs.value().is_dict());

  DemoSession* demo_session = DemoSession::Get();
  DCHECK(demo_session);

  // Adjust CRX paths in the prefs. Prefs on disk contains paths relative to
  // the offline demo resources root - they have to be changed to absolute paths
  // so extensions service knows from where to load them.
  for (auto&& dict_item : prefs.value().DictItems()) {
    if (!dict_item.second.is_dict())
      continue;

    const base::Value* path = dict_item.second.FindKeyOfType(
        extensions::ExternalProviderImpl::kExternalCrx,
        base::Value::Type::STRING);
    if (!path || !path->is_string())
      continue;

    base::FilePath relative_path = base::FilePath(path->GetString());
    if (relative_path.IsAbsolute()) {
      LOG(ERROR) << "Ignoring demo extension with an absolute path "
                 << dict_item.first;
      dict_item.second.RemoveKey(
          extensions::ExternalProviderImpl::kExternalCrx);
      continue;
    }

    dict_item.second.SetKey(
        extensions::ExternalProviderImpl::kExternalCrx,
        base::Value(demo_session->GetOfflineResourceAbsolutePath(relative_path)
                        .value()));
  }

  LoadFinished(base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(std::move(prefs.value()))));
}

}  // namespace chromeos
