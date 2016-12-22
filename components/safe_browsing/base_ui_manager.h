// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_BASE_UI_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_BASE_UI_MANAGER_H_

#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "components/security_interstitials/content/unsafe_resource.h"

class GURL;

namespace content {
class NavigationEntry;
class WebContents;
}  // namespace content

namespace safe_browsing {

// Construction needs to happen on the main thread.
class BaseSafeBrowsingUIManager
    : public base::RefCountedThreadSafe<BaseSafeBrowsingUIManager> {
 public:
  typedef security_interstitials::UnsafeResource UnsafeResource;

  // Observer class can be used to get notified when a SafeBrowsing hit
  // was found.
  class Observer {
   public:
    // The |resource| was classified as unsafe by SafeBrowsing, and is
    // not whitelisted.
    // The |resource| must not be accessed after OnSafeBrowsingHit returns.
    // This method will be called on the UI thread.
    virtual void OnSafeBrowsingHit(const UnsafeResource& resource) = 0;

   protected:
    Observer() {}
    virtual ~Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  BaseSafeBrowsingUIManager();

  // Called to stop or shutdown operations on the io_thread. This may be called
  // multiple times during the life of the UIManager. Should be called
  // on IO thread. If shutdown is true, the manager is disabled permanently.
  virtual void StopOnIOThread(bool shutdown);

  // Called on the UI thread to display an interstitial page.
  // |url| is the url of the resource that matches a safe browsing list.
  // If the request contained a chain of redirects, |url| is the last url
  // in the chain, and |original_url| is the first one (the root of the
  // chain). Otherwise, |original_url| = |url|.
  virtual void DisplayBlockingPage(const UnsafeResource& resource);

  // Log the user perceived delay caused by SafeBrowsing. This delay is the time
  // delta starting from when we would have started reading data from the
  // network, and ending when the SafeBrowsing check completes indicating that
  // the current page is 'safe'.
  virtual void LogPauseDelay(base::TimeDelta time);

  // Called on the IO thread by the ThreatDetails with the serialized
  // protocol buffer, so the service can send it over.
  virtual void SendSerializedThreatDetails(const std::string& serialized);

  // Report hits to the unsafe contents (malware, phishing, unsafe download URL)
  // to the server. Can only be called on UI thread.  If |post_data| is
  // non-empty, the request will be sent as a POST instead of a GET.
  // Will report only for UMA || is_extended_reporting.
  virtual void MaybeReportSafeBrowsingHit(
      const safe_browsing::HitReport& hit_report);

  // A convenience wrapper method for IsUrlWhitelistedOrPendingForWebContents.
  virtual bool IsWhitelisted(const UnsafeResource& resource);

  // Checks if we already displayed or are displaying an interstitial
  // for the top-level site |url| in a given WebContents. If
  // |whitelist_only|, it returns true only if the user chose to ignore
  // the interstitial. Otherwise, it returns true if an interstitial for
  // |url| is already displaying *or* if the user has seen an
  // interstitial for |url| before in this WebContents and proceeded
  // through it. Called on the UI thread.
  //
  // If the resource was found in the whitelist or pending for the
  // whitelist, |threat_type| will be set to the SBThreatType for which
  // the URL was first whitelisted.
  virtual bool IsUrlWhitelistedOrPendingForWebContents(
      const GURL& url,
      bool is_subresource,
      content::NavigationEntry* entry,
      content::WebContents* web_contents,
      bool whitelist_only,
      SBThreatType* threat_type);

  // The blocking page for |web_contents| on the UI thread has
  // completed, with |proceed| set to true if the user has chosen to
  // proceed through the blocking page and false
  // otherwise. |web_contents| is the WebContents that was displaying
  // the blocking page. |main_frame_url| is the top-level URL on which
  // the blocking page was displayed. If |proceed| is true,
  // |main_frame_url| is whitelisted so that the user will not see
  // another warning for that URL in this WebContents.
  virtual void OnBlockingPageDone(const std::vector<UnsafeResource>& resources,
                                  bool proceed,
                                  content::WebContents* web_contents,
                                  const GURL& main_frame_url);

  // Add and remove observers.  These methods must be invoked on the UI thread.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* remove);

 protected:
  virtual ~BaseSafeBrowsingUIManager();

  // Updates the whitelist URL set for |web_contents|. Called on the UI thread.
  void AddToWhitelistUrlSet(const GURL& whitelist_url,
                            content::WebContents* web_contents,
                            bool is_pending,
                            SBThreatType threat_type);

  // Call protocol manager on IO thread to report hits of unsafe contents.
  virtual void ReportSafeBrowsingHitOnIOThread(
      const safe_browsing::HitReport& hit_report);

  // Removes |whitelist_url| from the pending whitelist for
  // |web_contents|. Called on the UI thread.
  void RemoveFromPendingWhitelistUrlSet(const GURL& whitelist_url,
                                        content::WebContents* web_contents);

  // Ensures that |web_contents| has its whitelist set in its userdata
  static void EnsureWhitelistCreated(content::WebContents* web_contents);

  base::ObserverList<Observer> observer_list_;

 private:
  friend class base::RefCountedThreadSafe<BaseSafeBrowsingUIManager>;

  DISALLOW_COPY_AND_ASSIGN(BaseSafeBrowsingUIManager);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_BASE_UI_MANAGER_H_
