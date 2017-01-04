// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef COMPONENTS_SAFE_BROWSING_BASE_SAFE_BROWSING_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_BASE_SAFE_BROWSING_SERVICE_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"

namespace net {
class URLRequest;
class URLRequestContextGetter;
}

namespace safe_browsing {
struct ResourceRequestInfo;
class SafeBrowsingDatabaseManager;

// Construction needs to happen on the main thread.
// The BaseSafeBrowsingService owns both the UI and Database managers which do
// the heavylifting of safebrowsing service. Both of these managers stay
// alive until BaseSafeBrowsingService is destroyed, however, they are disabled
// permanently when Shutdown method is called.
class BaseSafeBrowsingService : public base::RefCountedThreadSafe<
                                    BaseSafeBrowsingService,
                                    content::BrowserThread::DeleteOnUIThread> {
 public:
  // Called on the UI thread to initialize the service.
  virtual void Initialize();

  // Called on the main thread to let us know that the io_thread is going away.
  virtual void ShutDown();

  // Get current enabled status. Must be called on IO thread.
  bool enabled() const {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    return enabled_;
  }

  virtual scoped_refptr<net::URLRequestContextGetter> url_request_context();

  // This returns either the v3 or the v4 database manager, depending on
  // the experiment settings.
  virtual const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager()
      const;

  // Observes resource requests made by the renderer and reports suspicious
  // activity.
  void OnResourceRequest(const net::URLRequest* request);

  // Type for subscriptions to SafeBrowsing service state.
  typedef base::CallbackList<void(void)>::Subscription StateSubscription;

  // Adds a listener for when SafeBrowsing preferences might have changed. To
  // get the current state, the callback should call
  // SafeBrowsingService::enabled_by_prefs(). Should only be called on the UI
  // thread.
  std::unique_ptr<StateSubscription> RegisterStateCallback(
      const base::Callback<void(void)>& callback);

 protected:
  // Creates the safe browsing service.  Need to initialize before using.
  BaseSafeBrowsingService();

  virtual ~BaseSafeBrowsingService();

  virtual SafeBrowsingDatabaseManager* CreateDatabaseManager();

  // Whether the service is running. 'enabled_' is used by
  // BaseSafeBrowsingService on the IO thread during normal operations.
  bool enabled_;

  // The database manager handles the database and download logic.  Accessed on
  // both UI and IO thread.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // Callbacks when SafeBrowsing state might have changed.
  // Should only be accessed on the UI thread.
  base::CallbackList<void(void)> state_callback_list_;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<BaseSafeBrowsingService>;

  // Process the observed resource requests on the UI thread.
  void ProcessResourceRequest(const ResourceRequestInfo& request);

  DISALLOW_COPY_AND_ASSIGN(BaseSafeBrowsingService);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_BASE_SAFE_BROWSING_SERVICE_H_
