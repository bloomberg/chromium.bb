// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "components/safe_browsing_db/hit_report.h"
#include "components/safe_browsing_db/util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_type.h"
#include "url/gurl.h"

namespace content {
class NavigationEntry;
class WebContents;
}  // namespace content

namespace safe_browsing {

class SafeBrowsingService;

// Construction needs to happen on the main thread.
class SafeBrowsingUIManager
    : public base::RefCountedThreadSafe<SafeBrowsingUIManager> {
 public:
  // Passed a boolean indicating whether or not it is OK to proceed with
  // loading an URL.
  typedef base::Callback<void(bool /*proceed*/)> UrlCheckCallback;

  // Structure used to pass parameters between the IO and UI thread when
  // interacting with the blocking page.
  struct UnsafeResource {
    UnsafeResource();
    UnsafeResource(const UnsafeResource& other);
    ~UnsafeResource();

    // Returns true if this UnsafeResource is a main frame load that was blocked
    // while the navigation is still pending. Note that a main frame hit may not
    // be blocking, eg. client side detection happens after the load is
    // committed.
    bool IsMainPageLoadBlocked() const;

    // Returns the NavigationEntry for this resource (for a main frame hit) or
    // for the page which contains this resource (for a subresource hit).
    // This method must only be called while the UnsafeResource is still
    // "valid".
    // I.e,
    //   For MainPageLoadBlocked resources, it must not be called if the load
    //   was aborted (going back or replaced with a different navigation),
    //   or resumed (proceeded through warning or matched whitelist).
    //   For non-MainPageLoadBlocked resources, it must not be called if any
    //   other navigation has committed (whether by going back or unrelated
    //   navigations), though a pending navigation is okay.
    content::NavigationEntry* GetNavigationEntryForResource() const;

    // Helper to build a getter for WebContents* from render frame id.
    static base::Callback<content::WebContents*(void)> GetWebContentsGetter(
        int render_process_host_id,
        int render_frame_id);

    GURL url;
    GURL original_url;
    std::vector<GURL> redirect_urls;
    bool is_subresource;
    bool is_subframe;
    SBThreatType threat_type;
    ThreatMetadata threat_metadata;
    UrlCheckCallback callback;  // This is called back on |callback_thread|.
    scoped_refptr<base::SingleThreadTaskRunner> callback_thread;
    base::Callback<content::WebContents*(void)> web_contents_getter;
    safe_browsing::ThreatSource threat_source;
  };

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

  explicit SafeBrowsingUIManager(
      const scoped_refptr<SafeBrowsingService>& service);

  // Called to stop or shutdown operations on the io_thread. This may be called
  // multiple times during the life of the UIManager. Should be called
  // on IO thread. If shutdown is true, the manager is disabled permanently.
  void StopOnIOThread(bool shutdown);

  // Called on the UI thread to display an interstitial page.
  // |url| is the url of the resource that matches a safe browsing list.
  // If the request contained a chain of redirects, |url| is the last url
  // in the chain, and |original_url| is the first one (the root of the
  // chain). Otherwise, |original_url| = |url|.
  virtual void DisplayBlockingPage(const UnsafeResource& resource);

  // A convenience wrapper method for IsUrlWhitelistedOrPendingForWebContents.
  bool IsWhitelisted(const UnsafeResource& resource);

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
  bool IsUrlWhitelistedOrPendingForWebContents(
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
  void OnBlockingPageDone(const std::vector<UnsafeResource>& resources,
                          bool proceed,
                          content::WebContents* web_contents,
                          const GURL& main_frame_url);

  // Log the user perceived delay caused by SafeBrowsing. This delay is the time
  // delta starting from when we would have started reading data from the
  // network, and ending when the SafeBrowsing check completes indicating that
  // the current page is 'safe'.
  void LogPauseDelay(base::TimeDelta time);

  // Called on the IO thread by the ThreatDetails with the serialized
  // protocol buffer, so the service can send it over.
  virtual void SendSerializedThreatDetails(const std::string& serialized);

  // Report hits to the unsafe contents (malware, phishing, unsafe download URL)
  // to the server. Can only be called on UI thread.  If |post_data| is
  // non-empty, the request will be sent as a POST instead of a GET.
  // Will report only for UMA || is_extended_reporting.
  virtual void MaybeReportSafeBrowsingHit(
      const safe_browsing::HitReport& hit_report);

  // Report an invalid TLS/SSL certificate chain to the server. Can only
  // be called on UI thread.
  void ReportInvalidCertificateChain(const std::string& serialized_report,
                                     const base::Closure& callback);

  // Report permission action to SafeBrowsing servers. Can only be called on UI
  // thread.
  void ReportPermissionAction(const PermissionReportInfo& report_info);

  // Add and remove observers.  These methods must be invoked on the UI thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* remove);

  // Creates the whitelist URL set for tests that create a blocking page
  // themselves and then simulate OnBlockingPageDone(). OnBlockingPageDone()
  // expects the whitelist to exist, but the tests don't necessarily call
  // DisplayBlockingPage(), which creates it.
  static void CreateWhitelistForTesting(content::WebContents* web_contents);

 protected:
  virtual ~SafeBrowsingUIManager();

 private:
  friend class base::RefCountedThreadSafe<SafeBrowsingUIManager>;
  friend class SafeBrowsingUIManagerTest;
  friend class TestSafeBrowsingUIManager;

  // Call protocol manager on IO thread to report hits of unsafe contents.
  void ReportSafeBrowsingHitOnIOThread(
      const safe_browsing::HitReport& hit_report);

  // Sends an invalid certificate chain report over the network.
  void ReportInvalidCertificateChainOnIOThread(
      const std::string& serialized_report);

  // Report permission action to SafeBrowsing servers.
  void ReportPermissionActionOnIOThread(
      const PermissionReportInfo& report_info);

  // Updates the whitelist URL set for |web_contents|. Called on the UI thread.
  void AddToWhitelistUrlSet(const GURL& whitelist_url,
                            content::WebContents* web_contents,
                            bool is_pending,
                            SBThreatType threat_type);

  // Removes |whitelist_url| from the pending whitelist for
  // |web_contents|. Called on the UI thread.
  void RemoveFromPendingWhitelistUrlSet(const GURL& whitelist_url,
                                        content::WebContents* web_contents);

  static GURL GetMainFrameWhitelistUrlForResourceForTesting(
      const safe_browsing::SafeBrowsingUIManager::UnsafeResource& resource);

  // Safebrowsing service.
  scoped_refptr<SafeBrowsingService> sb_service_;

  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUIManager);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_H_
