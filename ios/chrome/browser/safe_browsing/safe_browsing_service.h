// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner_helpers.h"
#include "ios/chrome/browser/safe_browsing/util.h"
#include "ios/web/public/web_thread.h"

class PrefChangeRegistrar;
class PrefService;
class TrackedPreferenceValidationDelegate;

namespace base {
class Thread;
}

namespace net {
class HttpResponseHeaders;
class URLRequest;
class URLRequestContextGetter;
}

namespace safe_browsing {
class ClientSideDetectionService;
class DownloadProtectionService;
struct SafeBrowsingProtocolConfig;
class SafeBrowsingPingManager;
class SafeBrowsingServiceFactory;
class SafeBrowsingUIManager;
class SafeBrowsingURLRequestContextGetter;

// Construction needs to happen on the main thread.
// The SafeBrowsingService owns both the UI and Database managers which do
// the heavylifting of safebrowsing service. Both of these managers stay
// alive until SafeBrowsingService is destroyed, however, they are disabled
// permanently when Shutdown method is called.
class SafeBrowsingService
    : public base::RefCountedThreadSafe<SafeBrowsingService,
                                        web::WebThread::DeleteOnUIThread> {
 public:
  // Makes the passed |factory| the factory used to instanciate
  // a SafeBrowsingService. Useful for tests.
  static void RegisterFactory(SafeBrowsingServiceFactory* factory) {
    factory_ = factory;
  }

  static base::FilePath GetBaseFilename();

  // Create an instance of the safe browsing service.
  static SafeBrowsingService* CreateSafeBrowsingService();

  // Called on the UI thread to initialize the service.
  void Initialize();

  // Called on the main thread to let us know that the io_thread is going away.
  void ShutDown();

  // Called on UI thread to decide if the download file's sha256 hash
  // should be calculated for safebrowsing.
  bool DownloadBinHashNeeded() const;

  // Create a protocol config struct.
  SafeBrowsingProtocolConfig GetProtocolConfig() const;

  // Get current enabled status. Must be called on IO thread.
  bool enabled() const {
    DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
    return enabled_;
  }

  // Whether the service is enabled by the current set of profiles.
  bool enabled_by_prefs() const {
    DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
    return enabled_by_prefs_;
  }

  net::URLRequestContextGetter* url_request_context();

  const scoped_refptr<SafeBrowsingUIManager>& ui_manager() const;

  SafeBrowsingPingManager* ping_manager() const;

  // Checks |headers| from a request that has passed through a proxy which
  // adds safe browsing headers, and returns the threat type indicated by those
  // headers (or SB_THREAT_TYPE_SAFE if no threat is indicated).
  SBThreatType CheckResponseFromProxyRequestHeaders(
      scoped_refptr<net::HttpResponseHeaders> headers);

  // Type for subscriptions to SafeBrowsing service state.
  typedef base::CallbackList<void(void)>::Subscription StateSubscription;

  // Adds a listener for when SafeBrowsing preferences might have changed.
  // To get the current state, the callback should call enabled_by_prefs().
  // Should only be called on the UI thread.
  scoped_ptr<StateSubscription> RegisterStateCallback(
      const base::Callback<void(void)>& callback);

  // Sends serialized download recovery report to backend.
  void SendDownloadRecoveryReport(const std::string& report);

 protected:
  // Creates the safe browsing service.  Need to initialize before using.
  SafeBrowsingService();

  virtual ~SafeBrowsingService();

  SafeBrowsingUIManager* CreateUIManager();

 private:
  friend class SafeBrowsingServiceFactoryImpl;
  friend struct web::WebThread::DeleteOnThread<web::WebThread::UI>;
  friend class base::DeleteHelper<SafeBrowsingService>;
  friend class SafeBrowsingURLRequestContextGetter;

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

  // Starts following the safe browsing preference on |pref_service|.
  void AddPrefService(PrefService* pref_service);

  // Stop following the safe browsing preference on |pref_service|.
  void RemovePrefService(PrefService* pref_service);

  // Checks if any profile is currently using the safe browsing service, and
  // starts or stops the service accordingly.
  void RefreshState();

  void OnSendDownloadRecoveryReport(const std::string& report);

  // The factory used to instanciate a SafeBrowsingService object.
  // Useful for tests, so they can provide their own implementation of
  // SafeBrowsingService.
  static SafeBrowsingServiceFactory* factory_;

  // The SafeBrowsingURLRequestContextGetter used to access
  // |url_request_context_|. Accessed on UI thread.
  scoped_refptr<SafeBrowsingURLRequestContextGetter>
      url_request_context_getter_;

  // Provides phishing and malware statistics. Accessed on IO thread.
  SafeBrowsingPingManager* ping_manager_;

  // Whether the service is running. 'enabled_' is used by SafeBrowsingService
  // on the IO thread during normal operations.
  bool enabled_;

  // Whether SafeBrowsing is enabled by the current set of profiles.
  // Accessed on UI thread.
  bool enabled_by_prefs_;

  // Tracks existing PrefServices, and the safe browsing preference on each.
  // This is used to determine if any profile is currently using the safe
  // browsing service, and to start it up or shut it down accordingly.
  // Accessed on UI thread.
  std::map<PrefService*, PrefChangeRegistrar*> prefs_map_;

  // Callbacks when SafeBrowsing state might have changed.
  // Should only be accessed on the UI thread.
  base::CallbackList<void(void)> state_callback_list_;

  // The UI manager handles showing interstitials.  Accessed on both UI and IO
  // thread.
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingService);
};

// Factory for creating SafeBrowsingService.  Useful for tests.
class SafeBrowsingServiceFactory {
 public:
  SafeBrowsingServiceFactory() {}
  virtual ~SafeBrowsingServiceFactory() {}
  virtual SafeBrowsingService* CreateSafeBrowsingService() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceFactory);
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
