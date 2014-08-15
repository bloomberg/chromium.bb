// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chrome/browser/safe_browsing/incident_reporting/delayed_analysis_callback.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefChangeRegistrar;
class PrefService;
class Profile;
struct SafeBrowsingProtocolConfig;
class SafeBrowsingDatabaseManager;
class SafeBrowsingPingManager;
class SafeBrowsingProtocolManager;
class SafeBrowsingServiceFactory;
class SafeBrowsingUIManager;
class SafeBrowsingURLRequestContextGetter;
class TrackedPreferenceValidationDelegate;

namespace base {
class Thread;
}

namespace net {
class URLRequestContext;
class URLRequestContextGetter;
}

namespace safe_browsing {
class ClientSideDetectionService;
class DownloadProtectionService;
class IncidentReportingService;
}

// Construction needs to happen on the main thread.
// The SafeBrowsingService owns both the UI and Database managers which do
// the heavylifting of safebrowsing service. Both of these managers stay
// alive until SafeBrowsingService is destroyed, however, they are disabled
// permanently when Shutdown method is called.
class SafeBrowsingService
    : public base::RefCountedThreadSafe<
          SafeBrowsingService,
          content::BrowserThread::DeleteOnUIThread>,
      public content::NotificationObserver {
 public:
  // Makes the passed |factory| the factory used to instanciate
  // a SafeBrowsingService. Useful for tests.
  static void RegisterFactory(SafeBrowsingServiceFactory* factory) {
    factory_ = factory;
  }

  static base::FilePath GetCookieFilePathForTesting();

  static base::FilePath GetBaseFilename();

  // Create an instance of the safe browsing service.
  static SafeBrowsingService* CreateSafeBrowsingService();

#if defined(OS_ANDROID) && defined(FULL_SAFE_BROWSING)
  // Return whether the user is in mobile safe browsing
  // field trial enabled group.
  static bool IsEnabledByFieldTrial();
#endif

  // Called on the UI thread to initialize the service.
  void Initialize();

  // Called on the main thread to let us know that the io_thread is going away.
  void ShutDown();

  // Called on UI thread to decide if the download file's sha256 hash
  // should be calculated for safebrowsing.
  bool DownloadBinHashNeeded() const;

  // Create a protocol config struct.
  virtual SafeBrowsingProtocolConfig GetProtocolConfig() const;

  bool enabled() const { return enabled_; }

  safe_browsing::ClientSideDetectionService*
      safe_browsing_detection_service() const {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    return csd_service_.get();
  }

  // The DownloadProtectionService is not valid after the SafeBrowsingService
  // is destroyed.
  safe_browsing::DownloadProtectionService*
      download_protection_service() const {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    return download_service_.get();
  }

  net::URLRequestContextGetter* url_request_context();

  const scoped_refptr<SafeBrowsingUIManager>& ui_manager() const;

  const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager() const;

  SafeBrowsingProtocolManager* protocol_manager() const;

  SafeBrowsingPingManager* ping_manager() const;

  // Returns a preference validation delegate that adds incidents to the
  // incident reporting service for validation failures. Returns NULL if the
  // service is not applicable for the given profile.
  scoped_ptr<TrackedPreferenceValidationDelegate>
      CreatePreferenceValidationDelegate(Profile* profile) const;

  // Registers |callback| to be run after some delay following process launch.
  // |callback| will be dropped if the service is not applicable for the
  // process.
  void RegisterDelayedAnalysisCallback(
      const safe_browsing::DelayedAnalysisCallback& callback);

 protected:
  // Creates the safe browsing service.  Need to initialize before using.
  SafeBrowsingService();

  virtual ~SafeBrowsingService();

  virtual SafeBrowsingDatabaseManager* CreateDatabaseManager();

  virtual SafeBrowsingUIManager* CreateUIManager();

  // Registers all the delayed analysis with the incident reporting service.
  // This is where you register your process-wide, profile-independent analysis.
  virtual void RegisterAllDelayedAnalysis();

 private:
  friend class SafeBrowsingServiceFactoryImpl;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<SafeBrowsingService>;
  friend class SafeBrowsingServerTest;
  friend class SafeBrowsingServiceTest;
  friend class SafeBrowsingURLRequestContextGetter;

  void InitURLRequestContextOnIOThread(
      net::URLRequestContextGetter* system_url_request_context_getter);

  void DestroyURLRequestContextOnIOThread();

  // Called to initialize objects that are used on the io_thread.  This may be
  // called multiple times during the life of the SafeBrowsingService.
  void StartOnIOThread(
      net::URLRequestContextGetter* url_request_context_getter);

  // Called to stop or shutdown operations on the io_thread. This may be called
  // multiple times to stop during the life of the SafeBrowsingService. If
  // shutdown is true, then the operations on the io thread are shutdown
  // permanently and cannot be restarted.
  void StopOnIOThread(bool shutdown);

  // Start up SafeBrowsing objects. This can be called at browser start, or when
  // the user checks the "Enable SafeBrowsing" option in the Advanced options
  // UI.
  void Start();

  // Stops the SafeBrowsingService. This can be called when the safe browsing
  // preference is disabled. When shutdown is true, operation is permanently
  // shutdown and cannot be restarted.
  void Stop(bool shutdown);

  // content::NotificationObserver override
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Starts following the safe browsing preference on |pref_service|.
  void AddPrefService(PrefService* pref_service);

  // Stop following the safe browsing preference on |pref_service|.
  void RemovePrefService(PrefService* pref_service);

  // Checks if any profile is currently using the safe browsing service, and
  // starts or stops the service accordingly.
  void RefreshState();

  // The factory used to instanciate a SafeBrowsingService object.
  // Useful for tests, so they can provide their own implementation of
  // SafeBrowsingService.
  static SafeBrowsingServiceFactory* factory_;

  // The SafeBrowsingURLRequestContextGetter used to access
  // |url_request_context_|. Accessed on UI thread.
  scoped_refptr<net::URLRequestContextGetter>
      url_request_context_getter_;

  // The SafeBrowsingURLRequestContext. Accessed on IO thread.
  scoped_ptr<net::URLRequestContext> url_request_context_;

  // Handles interaction with SafeBrowsing servers. Accessed on IO thread.
  SafeBrowsingProtocolManager* protocol_manager_;

  // Provides phishing and malware statistics. Accessed on IO thread.
  SafeBrowsingPingManager* ping_manager_;

  // Whether the service is running. 'enabled_' is used by SafeBrowsingService
  // on the IO thread during normal operations.
  bool enabled_;

  // Tracks existing PrefServices, and the safe browsing preference on each.
  // This is used to determine if any profile is currently using the safe
  // browsing service, and to start it up or shut it down accordingly.
  // Accessed on UI thread.
  std::map<PrefService*, PrefChangeRegistrar*> prefs_map_;

  // Used to track creation and destruction of profiles on the UI thread.
  content::NotificationRegistrar prefs_registrar_;

  // The ClientSideDetectionService is managed by the SafeBrowsingService,
  // since its running state and lifecycle depends on SafeBrowsingService's.
  // Accessed on UI thread.
  scoped_ptr<safe_browsing::ClientSideDetectionService> csd_service_;

  // The DownloadProtectionService is managed by the SafeBrowsingService,
  // since its running state and lifecycle depends on SafeBrowsingService's.
  // Accessed on UI thread.
  scoped_ptr<safe_browsing::DownloadProtectionService> download_service_;

  scoped_ptr<safe_browsing::IncidentReportingService> incident_service_;

  // The UI manager handles showing interstitials.  Accessed on both UI and IO
  // thread.
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;

  // The database manager handles the database and download logic.  Accessed on
  // both UI and IO thread.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingService);
};

// Factory for creating SafeBrowsingService.  Useful for tests.
class SafeBrowsingServiceFactory {
 public:
  SafeBrowsingServiceFactory() { }
  virtual ~SafeBrowsingServiceFactory() { }
  virtual SafeBrowsingService* CreateSafeBrowsingService() = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceFactory);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
