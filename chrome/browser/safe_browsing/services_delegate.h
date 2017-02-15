// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "chrome/browser/safe_browsing/incident_reporting/delayed_analysis_callback.h"
#include "components/user_prefs/tracked/tracked_preference_validation_delegate.h"

class Profile;

namespace content {
class DownloadManager;
}

namespace net {
class URLRequestContextGetter;
}

namespace safe_browsing {

class ClientSideDetectionService;
class DownloadProtectionService;
class IncidentReportingService;
class ResourceRequestDetector;
struct ResourceRequestInfo;
class SafeBrowsingService;
class SafeBrowsingDatabaseManager;
struct V4ProtocolConfig;

// Abstraction to help organize code for mobile vs full safe browsing modes.
// This helper class should be owned by a SafeBrowsingService, and it handles
// responsibilities for safe browsing service classes that may or may not exist
// for a given build config. e.g. No DownloadProtectionService on mobile.
// ServicesDelegate lives on the UI thread.
class ServicesDelegate {
 public:
  // Used for tests to override service creation. If CanCreateFooService()
  // returns true, then ServicesDelegate will use the service created by
  // CreateFooService(). If CanCreateFooService() returns false, then
  // ServicesDelegate will use its built-in service creation code.
  class ServicesCreator {
   public:
    virtual bool CanCreateDownloadProtectionService() = 0;
    virtual bool CanCreateIncidentReportingService() = 0;
    virtual bool CanCreateResourceRequestDetector() = 0;

    // Caller takes ownership of the returned object. Cannot use std::unique_ptr
    // because services may not be implemented for some build configs.
    virtual DownloadProtectionService* CreateDownloadProtectionService() = 0;
    virtual IncidentReportingService* CreateIncidentReportingService() = 0;
    virtual ResourceRequestDetector* CreateResourceRequestDetector() = 0;
  };

  // Creates the ServicesDelegate using its's default ServicesCreator.
  // |safe_browsing_service| is the delegate's owner.
  static std::unique_ptr<ServicesDelegate> Create(
      SafeBrowsingService* safe_browsing_service);

  // Creates the ServicesDelegate using a custom ServicesCreator, for testing.
  static std::unique_ptr<ServicesDelegate> CreateForTest(
      SafeBrowsingService* safe_browsing_service,
      ServicesDelegate::ServicesCreator* services_creator);

  virtual ~ServicesDelegate() {}

  virtual const scoped_refptr<SafeBrowsingDatabaseManager>&
  v4_local_database_manager() const = 0;

  // Initializes internal state using the ServicesCreator.
  virtual void Initialize(bool v4_enabled = false) = 0;

  // Creates the CSD service for the given |context_getter|.
  virtual void InitializeCsdService(
      net::URLRequestContextGetter* context_getter) = 0;

  // Shuts down the download service.
  virtual void ShutdownServices() = 0;

  // Handles SafeBrowsingService::RefreshState() for the provided services.
  virtual void RefreshState(bool enable) = 0;

  // See the SafeBrowsingService methods of the same name.
  virtual void ProcessResourceRequest(const ResourceRequestInfo* request) = 0;
  virtual std::unique_ptr<TrackedPreferenceValidationDelegate>
      CreatePreferenceValidationDelegate(Profile* profile) = 0;
  virtual void RegisterDelayedAnalysisCallback(
      const DelayedAnalysisCallback& callback) = 0;
  virtual void AddDownloadManager(
      content::DownloadManager* download_manager) = 0;

  // Returns nullptr for any service that is not available.
  virtual ClientSideDetectionService* GetCsdService() = 0;
  virtual DownloadProtectionService* GetDownloadService() = 0;

  virtual void StartOnIOThread(
    net::URLRequestContextGetter* url_request_context_getter,
    const V4ProtocolConfig& v4_config) = 0;
  virtual void StopOnIOThread(bool shutdown) = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_H_
