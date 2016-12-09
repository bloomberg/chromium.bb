// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_THREAT_DETAILS_HISTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_THREAT_DETAILS_HISTORY_H_

// This class gets redirect chain for urls from the history service.

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/base/completion_callback.h"

class Profile;

namespace safe_browsing {

typedef std::vector<GURL> RedirectChain;

class ThreatDetailsRedirectsCollector
    : public base::RefCountedThreadSafe<
          ThreatDetailsRedirectsCollector,
          content::BrowserThread::DeleteOnUIThread>,
      public content::NotificationObserver {
 public:
  explicit ThreatDetailsRedirectsCollector(Profile* profile);

  // Collects urls' redirects chain information from the history service.
  // We get access to history service via web_contents in UI thread.
  // Notice the callback will be posted to the IO thread.
  void StartHistoryCollection(const std::vector<GURL>& urls,
                              const base::Closure& callback);

  // Returns whether or not StartCacheCollection has been called.
  bool HasStarted() const;

  // Returns the redirect urls we get from history service
  const std::vector<RedirectChain>& GetCollectedUrls() const;

  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<ThreatDetailsRedirectsCollector>;

  ~ThreatDetailsRedirectsCollector() override;

  void StartGetRedirects(const std::vector<GURL>& urls);
  void GetRedirects(const GURL& url);
  void OnGotQueryRedirectsTo(const GURL& url,
                             const history::RedirectList* redirect_list);

  // Posts the callback method back to IO thread when redirects collecting
  // is all done.
  void AllDone();

  Profile* profile_;
  base::CancelableTaskTracker request_tracker_;

  // Method we call when we are done. The caller must be alive for the
  // whole time, we are modifying its state (see above).
  base::Closure callback_;

  // Sets to true once StartHistoryCollection is called
  bool has_started_;

  // The urls we need to get redirects for
  std::vector<GURL> urls_;
  // The iterator goes over urls_
  std::vector<GURL>::iterator urls_it_;
  // The collected directs from history service
  std::vector<RedirectChain> redirects_urls_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ThreatDetailsRedirectsCollector);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_THREAT_DETAILS_HISTORY_H_
