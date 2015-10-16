// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/application_context_impl.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/time/default_tick_clock.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/net_log/chrome_net_log.h"
#include "components/network_time/network_time_tracker.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/variations/service/variations_service.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/prefs/ios_chrome_pref_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "net/log/net_log_capture_mode.h"
#include "net/socket/client_socket_pool_manager.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/policy_service.h"
#else
#include "components/policy/core/common/policy_service_stub.h"
#endif

ApplicationContextImpl::ApplicationContextImpl(
    base::SequencedTaskRunner* local_state_task_runner,
    const base::CommandLine& command_line)
    : local_state_task_runner_(local_state_task_runner),
      was_last_shutdown_clean_(false),
      created_local_state_(false) {
  DCHECK(!GetApplicationContext());
  SetApplicationContext(this);

  net_log_.reset(new net_log::ChromeNetLog(
      base::FilePath(), net::NetLogCaptureMode::Default(),
      command_line.GetCommandLineString(), GetChannelString()));
}

ApplicationContextImpl::~ApplicationContextImpl() {
  DCHECK_EQ(this, GetApplicationContext());
  SetApplicationContext(nullptr);
}

// static
void ApplicationContextImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(metrics::prefs::kMetricsReportingEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kLastSessionExitedCleanly, true);
  registry->RegisterBooleanPref(prefs::kMetricsReportingWifiOnly, true);
}

void ApplicationContextImpl::SetApplicationLocale(const std::string& locale) {
  DCHECK(thread_checker_.CalledOnValidThread());
  application_locale_ = locale;
  translate::TranslateDownloadManager::GetInstance()->set_application_locale(
      application_locale_);
}

void ApplicationContextImpl::OnAppEnterForeground() {
  DCHECK(thread_checker_.CalledOnValidThread());

  PrefService* local_state = GetLocalState();
  local_state->SetBoolean(prefs::kLastSessionExitedCleanly, false);

  // Tell the metrics service that the application resumes.
  metrics::MetricsService* metrics_service = GetMetricsService();
  if (metrics_service && local_state) {
    metrics_service->OnAppEnterForeground();
    local_state->CommitPendingWrite();
  }

  variations::VariationsService* variations_service = GetVariationsService();
  if (variations_service)
    variations_service->OnAppEnterForeground();

  std::vector<ios::ChromeBrowserState*> loaded_browser_state =
      GetChromeBrowserStateManager()->GetLoadedChromeBrowserStates();
  for (ios::ChromeBrowserState* browser_state : loaded_browser_state) {
    browser_state->SetExitType(ios::ChromeBrowserState::EXIT_CRASHED);
  }
}

void ApplicationContextImpl::OnAppEnterBackground() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Mark all the ChromeBrowserStates as clean and persist history.
  std::vector<ios::ChromeBrowserState*> loaded_browser_state =
      GetChromeBrowserStateManager()->GetLoadedChromeBrowserStates();
  for (ios::ChromeBrowserState* browser_state : loaded_browser_state) {
    if (history::HistoryService* history_service =
            ios::HistoryServiceFactory::GetForBrowserStateIfExists(
                browser_state, ServiceAccessType::EXPLICIT_ACCESS)) {
      history_service->HandleBackgrounding();
    }

    browser_state->SetExitType(ios::ChromeBrowserState::EXIT_NORMAL);
    PrefService* browser_state_prefs = browser_state->GetPrefs();
    if (browser_state_prefs)
      browser_state_prefs->CommitPendingWrite();
  }

  PrefService* local_state = GetLocalState();
  local_state->SetBoolean(prefs::kLastSessionExitedCleanly, true);

  // Tell the metrics service it was cleanly shutdown.
  metrics::MetricsService* metrics_service = GetMetricsService();
  if (metrics_service && local_state)
    metrics_service->OnAppEnterBackground();

  // Persisting to disk is protected by a critical task, so no other special
  // handling is necessary on iOS.
}

bool ApplicationContextImpl::WasLastShutdownClean() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Make sure the locale state is created as the file is initialized there.
  ignore_result(GetLocalState());
  return was_last_shutdown_clean_;
}

PrefService* ApplicationContextImpl::GetLocalState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!created_local_state_)
    CreateLocalState();
  return local_state_.get();
}

net::URLRequestContextGetter*
ApplicationContextImpl::GetSystemURLRequestContext() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ios::GetChromeBrowserProvider()->GetSystemURLRequestContext();
}

const std::string& ApplicationContextImpl::GetApplicationLocale() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!application_locale_.empty());
  return application_locale_;
}

ios::ChromeBrowserStateManager*
ApplicationContextImpl::GetChromeBrowserStateManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ios::GetChromeBrowserProvider()->GetChromeBrowserStateManager();
}

metrics::MetricsService* ApplicationContextImpl::GetMetricsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ios::GetChromeBrowserProvider()->GetMetricsService();
}

variations::VariationsService* ApplicationContextImpl::GetVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ios::GetChromeBrowserProvider()->GetVariationsService();
}

policy::BrowserPolicyConnector*
ApplicationContextImpl::GetBrowserPolicyConnector() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ios::GetChromeBrowserProvider()->GetBrowserPolicyConnector();
}

policy::PolicyService* ApplicationContextImpl::GetPolicyService() {
  DCHECK(thread_checker_.CalledOnValidThread());
#if defined(ENABLE_CONFIGURATION_POLICY)
  return GetBrowserPolicyConnector()->GetPolicyService();
#else
  if (!policy_service_)
    policy_service_.reset(new policy::PolicyServiceStub);
  return policy_service_.get();
#endif
}

rappor::RapporService* ApplicationContextImpl::GetRapporService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ios::GetChromeBrowserProvider()->GetRapporService();
}

net_log::ChromeNetLog* ApplicationContextImpl::GetNetLog() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return net_log_.get();
}

network_time::NetworkTimeTracker*
ApplicationContextImpl::GetNetworkTimeTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!network_time_tracker_) {
    network_time_tracker_.reset(new network_time::NetworkTimeTracker(
        make_scoped_ptr(new base::DefaultTickClock), GetLocalState()));
  }
  return network_time_tracker_.get();
}

void ApplicationContextImpl::CreateLocalState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!created_local_state_ && !local_state_);
  created_local_state_ = true;

  base::FilePath local_state_path;
  CHECK(PathService::Get(ios::FILE_LOCAL_STATE, &local_state_path));
  scoped_refptr<PrefRegistrySimple> pref_registry(new PrefRegistrySimple);

  // Register local state preferences.
  RegisterLocalStatePrefs(pref_registry.get());

  local_state_ =
      ::CreateLocalState(local_state_path, local_state_task_runner_.get(),
                         GetPolicyService(), pref_registry, false);

  const int max_per_proxy =
      local_state_->GetInteger(ios::prefs::kMaxConnectionsPerProxy);
  net::ClientSocketPoolManager::set_max_sockets_per_proxy_server(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL,
      std::max(std::min(max_per_proxy, 99),
               net::ClientSocketPoolManager::max_sockets_per_group(
                   net::HttpNetworkSession::NORMAL_SOCKET_POOL)));

  // Register the shutdown state before anything changes it.
  if (local_state_->HasPrefPath(prefs::kLastSessionExitedCleanly)) {
    was_last_shutdown_clean_ =
        local_state_->GetBoolean(prefs::kLastSessionExitedCleanly);
  }
}
