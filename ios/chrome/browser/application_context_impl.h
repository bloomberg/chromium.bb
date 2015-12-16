// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APPLICATION_CONTEXT_IMPL_H_
#define IOS_CHROME_BROWSER_APPLICATION_CONTEXT_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "ios/chrome/browser/application_context.h"

class PrefRegistrySimple;

namespace base {
class CommandLine;
class SequencedTaskRunner;
}

class ApplicationContextImpl : public ApplicationContext {
 public:
  ApplicationContextImpl(base::SequencedTaskRunner* local_state_task_runner,
                         const base::CommandLine& command_line,
                         const std::string& locale);
  ~ApplicationContextImpl() override;

  // Registers local state prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Called before the browser threads are created.
  void PreCreateThreads();

  // Called after the threads have been created but before the message loops
  // starts running. Allows the ApplicationContext to do any initialization
  // that requres all threads running.
  void PreMainMessageLoopRun();

  // Most cleanup is done by these functions, driven from IOSChromeMainParts
  // rather than in the destructor, so that we can interleave cleanup with
  // threads being stopped.
  void StartTearDown();
  void PostDestroyThreads();

  // ApplicationContext implementation.
  void OnAppEnterForeground() override;
  void OnAppEnterBackground() override;
  bool WasLastShutdownClean() override;
  PrefService* GetLocalState() override;
  net::URLRequestContextGetter* GetSystemURLRequestContext() override;
  const std::string& GetApplicationLocale() override;
  ios::ChromeBrowserStateManager* GetChromeBrowserStateManager() override;
  metrics_services_manager::MetricsServicesManager* GetMetricsServicesManager()
      override;
  metrics::MetricsService* GetMetricsService() override;
  variations::VariationsService* GetVariationsService() override;
  rappor::RapporService* GetRapporService() override;
  net_log::ChromeNetLog* GetNetLog() override;
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  IOSChromeIOThread* GetIOSChromeIOThread() override;
  gcm::GCMDriver* GetGCMDriver() override;
  web_resource::PromoResourceService* GetPromoResourceService() override;
  component_updater::ComponentUpdateService* GetComponentUpdateService()
      override;
  CRLSetFetcher* GetCRLSetFetcher() override;
  safe_browsing::SafeBrowsingService* GetSafeBrowsingService() override;

 private:
  // Sets the locale used by the application.
  void SetApplicationLocale(const std::string& locale);

  // Create the local state.
  void CreateLocalState();

  // Create the gcm driver.
  void CreateGCMDriver();

  base::ThreadChecker thread_checker_;
  scoped_ptr<PrefService> local_state_;
  scoped_ptr<net_log::ChromeNetLog> net_log_;
  scoped_ptr<network_time::NetworkTimeTracker> network_time_tracker_;
  scoped_ptr<IOSChromeIOThread> ios_chrome_io_thread_;
  scoped_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;
  scoped_ptr<gcm::GCMDriver> gcm_driver_;
  scoped_ptr<web_resource::PromoResourceService> promo_resource_service_;
  scoped_ptr<component_updater::ComponentUpdateService> component_updater_;
  scoped_refptr<CRLSetFetcher> crl_set_fetcher_;
  scoped_refptr<safe_browsing::SafeBrowsingService> safe_browsing_service_;
  std::string application_locale_;

  // Sequenced task runner for local state related I/O tasks.
  const scoped_refptr<base::SequencedTaskRunner> local_state_task_runner_;

  bool was_last_shutdown_clean_;
  bool created_local_state_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationContextImpl);
};

#endif  // IOS_CHROME_BROWSER_APPLICATION_CONTEXT_IMPL_H_
