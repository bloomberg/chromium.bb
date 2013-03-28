// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_NAVIGATION_OBSERVER_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class InfoBarDelegate;
class ManagedModeURLFilter;
class ManagedUserService;

class ManagedModeNavigationObserver
    : public content::WebContentsUserData<ManagedModeNavigationObserver>,
      public content::WebContentsObserver,
      public content::NotificationObserver {
 public:
  typedef base::Callback<void(bool)> SuccessCallback;

  virtual ~ManagedModeNavigationObserver();

  // Called when a network request was blocked. The passed in |callback| is
  // called with the result of the interstitial, i.e. whether we should proceed
  // with the request or not.
  static void DidBlockRequest(int render_process_id,
                              int render_view_id,
                              const GURL& url,
                              const SuccessCallback& callback);

  // Sets the specific infobar as dismissed.
  void WarnInfobarDismissed();
  void PreviewInfobarDismissed();

  // Returns whether the user should be allowed to navigate to this URL after
  // he has clicked "Preview" on the interstitial.
  bool CanTemporarilyNavigateHost(const GURL& url);

  // Clears the state recorded in the observer.
  void ClearObserverState();

  // Whitelists exact URLs for redirects and host patterns for the final URL.
  // If the URL uses HTTPS, whitelists only the host on HTTPS. Clears the
  // observer state after adding the URLs.
  void AddSavedURLsToWhitelistAndClearState();

  // Returns the elevation state for the corresponding WebContents.
  bool is_elevated() const;

  // Set the elevation state for the corresponding WebContents.
  void set_elevated(bool is_elevated);

 private:
  // An observer can be in one of the following states:
  // - RECORDING_URLS_BEFORE_PREVIEW: This is the initial state when the user
  // navigates to a new page. In this state the navigated URLs that should be
  // blocked are recorded. The Observer moves to the next state when an
  // interstitial was shown and the user clicked "Preview" on the interstitial.
  // - RECORDING_URLS_AFTER_PREVIEW: URLs that should be blocked are
  // still recorded while the page redirects. The Observer moves to the next
  // state after the page finished redirecting (DidNavigateMainFrame gets
  // called).
  enum ObserverState {
    RECORDING_URLS_BEFORE_PREVIEW,
    RECORDING_URLS_AFTER_PREVIEW,
  };

  friend class content::WebContentsUserData<ManagedModeNavigationObserver>;

  explicit ManagedModeNavigationObserver(content::WebContents* web_contents);

  // Shows the blocking interstitial if it is not already shown.
  void ShowInterstitial(const GURL& url);

  // Queues up a callback to be called with the result of the interstitial.
  void AddInterstitialCallback(const GURL& url,
                               const SuccessCallback& callback);

  // Dispatches the result of the interstitial to all pending callbacks.
  void OnInterstitialResult(bool result);

  // Adding the temporary exception stops the ResourceThrottle from showing
  // an interstitial for this RenderView. This allows the user to navigate
  // around on the website after clicking preview.
  void AddTemporaryException();
  // Updates the ResourceThrottle with the latest user navigation status.
  void UpdateExceptionNavigationStatus();
  void RemoveTemporaryException();

  void AddURLToPatternList(const GURL& url);

  // Returns whether the user would stay in elevated state if he visits this
  // URL.
  bool ShouldStayElevatedForURL(const GURL& url);

  // content::WebContentsObserver implementation.
  // An example regarding the order in which these events take place for
  // google.com in our case is as follows:
  // 1. User types in google.com and clicks enter.
  //  -> NavigateToPendingEntry: http://google.com
  //  -> DidStartProvisionalLoadForFrame http://google.com
  //  -> ProvisionalChangeToMainFrameUrl http://google.com
  // 2. Interstitial is shown to the user. User clicks "Preview".
  //  -> ProvisionalChangeToMainFrameUrl http://www.google.com (redirect)
  //  -> DidCommitProvisionalLoadForFrame http://www.google.com (redirect)
  //  -> DidNavigateMainFrame http://www.google.com
  // 3. Page is shown.
  virtual void NavigateToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) OVERRIDE;
  virtual void ProvisionalChangeToMainFrameUrl(
      const GURL& url,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      content::PageTransition transition_type,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void DidGetUserGesture() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Owned by the profile, so outlives us.
  ManagedUserService* managed_user_service_;

  // Owned by ManagedUserService.
  const ManagedModeURLFilter* url_filter_;

  // Owned by the InfoBarService, which has the same lifetime as this object.
  InfoBarDelegate* warn_infobar_delegate_;
  InfoBarDelegate* preview_infobar_delegate_;

  // Whether we received a user gesture.
  // The goal is to allow automatic redirects (in order not to break the flow
  // or show too many interstitials) while not allowing the user to navigate
  // to blocked pages. We consider a redirect to be automatic if we did
  // not get a user gesture.
  bool got_user_gesture_;
  ObserverState state_;
  std::set<GURL> navigated_urls_;
  GURL last_url_;

  // The elevation state corresponding to the current WebContents.
  // Will be set to true for non-managed users.
  bool is_elevated_;

  base::WeakPtrFactory<ManagedModeNavigationObserver> weak_ptr_factory_;

  // These callbacks get queued up when we show the blocking interstitial and
  // are posted to the IO thread with the result of the interstitial, i.e.
  // whether the user successfully authenticated to continue.
  std::vector<SuccessCallback> callbacks_;

  content::NotificationRegistrar registrar_;

  int last_allowed_page_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeNavigationObserver);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_NAVIGATION_OBSERVER_H_
