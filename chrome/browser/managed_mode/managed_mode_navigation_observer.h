// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_NAVIGATION_OBSERVER_H_

#include <set>

#include "base/values.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class InfoBarDelegate;
class ManagedModeURLFilter;

class ManagedModeNavigationObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ManagedModeNavigationObserver> {
 public:
  virtual ~ManagedModeNavigationObserver();

  // Sets the specific infobar as dismissed.
  void WarnInfobarDismissed();
  void PreviewInfobarDismissed();

  // Sets the state of the Observer from the outside.
  void SetStateToRecordingAfterPreview();

  // Returns whether the user should be allowed to navigate to this URL after
  // he has clicked "Preview" on the interstitial.
  bool CanTemporarilyNavigateHost(const GURL& url);

  // Clears the state recorded in the observer.
  void ClearObserverState();

  // Whitelists exact URLs for redirects and host patterns for the final URL.
  // If the URL uses HTTPS, whitelists only the host on HTTPS. Clears the
  // observer state after adding the URLs.
  void AddSavedURLsToWhitelistAndClearState();

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
  // - NOT_RECORDING_URLS: The redirects have completed and blocked URLs are
  // no longer being recorded.
  enum ObserverState {
    RECORDING_URLS_BEFORE_PREVIEW,
    RECORDING_URLS_AFTER_PREVIEW,
    NOT_RECORDING_URLS,
  };

  friend class content::WebContentsUserData<ManagedModeNavigationObserver>;

  explicit ManagedModeNavigationObserver(content::WebContents* web_contents);

  // Adding the temporary exception stops the ResourceThrottle from showing
  // an interstitial for this RenderView. This allows the user to navigate
  // around on the website after clicking preview.
  void AddTemporaryException();
  void RemoveTemporaryException();

  void AddURLToPatternList(const GURL& url);
  void AddURLAsLastPattern(const GURL& url);

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
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      int64 parent_frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void ProvisionalChangeToMainFrameUrl(
      const GURL& url,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      content::PageTransition transition_type,
      content::RenderViewHost* render_view_host) OVERRIDE;

  // Owned by ManagedMode (which is a singleton and outlives us).
  const ManagedModeURLFilter* url_filter_;

  // Owned by the InfoBarService, which has the same lifetime as this object.
  InfoBarDelegate* warn_infobar_delegate_;
  InfoBarDelegate* preview_infobar_delegate_;

  ObserverState state_;
  std::set<GURL> navigated_urls_;
  GURL last_url_;

  int last_allowed_page_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeNavigationObserver);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_NAVIGATION_OBSERVER_H_
