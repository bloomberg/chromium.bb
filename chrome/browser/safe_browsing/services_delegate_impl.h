// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"
#include "chrome/browser/safe_browsing/incident_reporting/resource_request_detector.h"
#include "chrome/browser/safe_browsing/services_delegate.h"

namespace safe_browsing {

class V4LocalDatabaseManager;

// Actual ServicesDelegate implementation. Create via
// ServicesDelegate::Create().
class ServicesDelegateImpl : public ServicesDelegate {
 public:
  ServicesDelegateImpl(SafeBrowsingService* safe_browsing_service,
                       ServicesDelegate::ServicesCreator* services_creator);
  ~ServicesDelegateImpl() override;

 private:
  // ServicesDelegate:
  void Initialize() override;
  void InitializeCsdService(
      net::URLRequestContextGetter* context_getter) override;
  void ShutdownServices() override;
  void RefreshState(bool enable) override;
  void ProcessResourceRequest(const ResourceRequestInfo* request) override;
  std::unique_ptr<TrackedPreferenceValidationDelegate>
      CreatePreferenceValidationDelegate(Profile* profile) override;
  void RegisterDelayedAnalysisCallback(
      const DelayedAnalysisCallback& callback) override;
  void RegisterExtendedReportingOnlyDelayedAnalysisCallback(
      const DelayedAnalysisCallback& callback) override;
  void AddDownloadManager(content::DownloadManager* download_manager) override;
  ClientSideDetectionService* GetCsdService() override;
  DownloadProtectionService* GetDownloadService() override;

  void StartOnIOThread(
    net::URLRequestContextGetter* url_request_context_getter,
    const V4ProtocolConfig& v4_config) override;
  void StopOnIOThread(bool shutdown) override;

  // Is the Pver4 database manager enabled? Controlled by Finch.
  bool IsV4LocalDatabaseManagerEnabled();

  V4LocalDatabaseManager* CreateV4LocalDatabaseManager();

  DownloadProtectionService* CreateDownloadProtectionService();
  IncidentReportingService* CreateIncidentReportingService();
  ResourceRequestDetector* CreateResourceRequestDetector();

  std::unique_ptr<ClientSideDetectionService> csd_service_;
  std::unique_ptr<DownloadProtectionService> download_service_;
  std::unique_ptr<IncidentReportingService> incident_service_;
  std::unique_ptr<ResourceRequestDetector> resource_request_detector_;

  SafeBrowsingService* const safe_browsing_service_;
  ServicesDelegate::ServicesCreator* const services_creator_;

  // The Pver4 local database manager handles the database and download logic
  // Accessed on both UI and IO thread.
  scoped_refptr<V4LocalDatabaseManager> v4_local_database_manager_;

  DISALLOW_COPY_AND_ASSIGN(ServicesDelegateImpl);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_IMPL_H_
