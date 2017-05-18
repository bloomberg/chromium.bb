// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"

#include <string>
#include <vector>

#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/component_patcher_operation_out_of_process.h"
#include "chrome/browser/component_updater/component_updater_utils.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/configurator_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_query_params.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "chrome/installer/util/google_update_settings.h"
#endif

namespace component_updater {

namespace {

class ChromeConfigurator : public update_client::Configurator {
 public:
  ChromeConfigurator(const base::CommandLine* cmdline,
                     net::URLRequestContextGetter* url_request_getter,
                     PrefService* pref_service);

  // update_client::Configurator overrides.
  int InitialDelay() const override;
  int NextCheckDelay() const override;
  int OnDemandDelay() const override;
  int UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  std::string GetProdId() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetBrand() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  std::string ExtraRequestParams() const override;
  std::string GetDownloadPreference() const override;
  net::URLRequestContextGetter* RequestContext() const override;
  scoped_refptr<update_client::OutOfProcessPatcher> CreateOutOfProcessPatcher()
      const override;
  bool EnabledDeltas() const override;
  bool EnabledComponentUpdates() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner()
      const override;
  PrefService* GetPrefService() const override;
  bool IsPerUserInstall() const override;

 private:
  friend class base::RefCountedThreadSafe<ChromeConfigurator>;

  ConfiguratorImpl configurator_impl_;
  PrefService* pref_service_;  // This member is not owned by this class.

  ~ChromeConfigurator() override {}
};

// Allows the component updater to use non-encrypted communication with the
// update backend. The security of the update checks is enforced using
// a custom message signing protocol and it does not depend on using HTTPS.
ChromeConfigurator::ChromeConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* url_request_getter,
    PrefService* pref_service)
    : configurator_impl_(cmdline, url_request_getter, false),
      pref_service_(pref_service) {
  DCHECK(pref_service_);
}

int ChromeConfigurator::InitialDelay() const {
  return configurator_impl_.InitialDelay();
}

int ChromeConfigurator::NextCheckDelay() const {
  return configurator_impl_.NextCheckDelay();
}

int ChromeConfigurator::OnDemandDelay() const {
  return configurator_impl_.OnDemandDelay();
}

int ChromeConfigurator::UpdateDelay() const {
  return configurator_impl_.UpdateDelay();
}

std::vector<GURL> ChromeConfigurator::UpdateUrl() const {
  return configurator_impl_.UpdateUrl();
}

std::vector<GURL> ChromeConfigurator::PingUrl() const {
  return configurator_impl_.PingUrl();
}

std::string ChromeConfigurator::GetProdId() const {
  return update_client::UpdateQueryParams::GetProdIdString(
      update_client::UpdateQueryParams::ProdId::CHROME);
}

base::Version ChromeConfigurator::GetBrowserVersion() const {
  return configurator_impl_.GetBrowserVersion();
}

std::string ChromeConfigurator::GetChannel() const {
  return chrome::GetChannelString();
}

std::string ChromeConfigurator::GetBrand() const {
  std::string brand;
  google_brand::GetBrand(&brand);
  return brand;
}

std::string ChromeConfigurator::GetLang() const {
  return ChromeUpdateQueryParamsDelegate::GetLang();
}

std::string ChromeConfigurator::GetOSLongName() const {
  return configurator_impl_.GetOSLongName();
}

std::string ChromeConfigurator::ExtraRequestParams() const {
  return configurator_impl_.ExtraRequestParams();
}

std::string ChromeConfigurator::GetDownloadPreference() const {
#if defined(OS_WIN)
  // This group policy is supported only on Windows and only for enterprises.
  return base::win::IsEnterpriseManaged()
             ? base::SysWideToUTF8(
                   GoogleUpdateSettings::GetDownloadPreference())
             : std::string();
#else
  return std::string();
#endif
}

net::URLRequestContextGetter* ChromeConfigurator::RequestContext() const {
  return configurator_impl_.RequestContext();
}

scoped_refptr<update_client::OutOfProcessPatcher>
ChromeConfigurator::CreateOutOfProcessPatcher() const {
  return make_scoped_refptr(new ChromeOutOfProcessPatcher);
}

bool ChromeConfigurator::EnabledDeltas() const {
  return configurator_impl_.EnabledDeltas();
}

bool ChromeConfigurator::EnabledComponentUpdates() const {
  return pref_service_->GetBoolean(prefs::kComponentUpdatesEnabled);
}

bool ChromeConfigurator::EnabledBackgroundDownloader() const {
  return configurator_impl_.EnabledBackgroundDownloader();
}

bool ChromeConfigurator::EnabledCupSigning() const {
  return configurator_impl_.EnabledCupSigning();
}

// Returns a task runner to run blocking tasks. The task runner continues to run
// after the browser shuts down, until the OS terminates the process. This
// imposes certain requirements for the code using the task runner, such as
// not accessing any global browser state while the code is running.
scoped_refptr<base::SequencedTaskRunner>
ChromeConfigurator::GetSequencedTaskRunner() const {
  return base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
}

PrefService* ChromeConfigurator::GetPrefService() const {
  DCHECK(pref_service_);
  return pref_service_;
}

bool ChromeConfigurator::IsPerUserInstall() const {
  return component_updater::IsPerUserInstall();
}

}  // namespace

void RegisterPrefsForChromeComponentUpdaterConfigurator(
    PrefRegistrySimple* registry) {
  // The component updates are enabled by default, if the preference is not set.
  registry->RegisterBooleanPref(prefs::kComponentUpdatesEnabled, true);
}

scoped_refptr<update_client::Configurator>
MakeChromeComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* context_getter,
    PrefService* pref_service) {
  return base::MakeRefCounted<ChromeConfigurator>(cmdline, context_getter,
                                                  pref_service);
}

}  // namespace component_updater
