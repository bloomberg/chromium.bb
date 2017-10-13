// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_SAFE_BROWSING_TRIGGERED_POPUP_BLOCKER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_SAFE_BROWSING_TRIGGERED_POPUP_BLOCKER_H_

#include <memory>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "components/safe_browsing/db/util.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
struct OpenURLParams;
class WebContents;
}  // namespace content

extern const base::Feature kAbusiveExperienceEnforce;

// This class observes main frame navigation checks incoming from safe browsing
// (currently implemented by the subresource_filter component). For navigations
// which match the ABUSIVE safe browsing list, this class will help the popup
// tab helper in applying a stronger policy for blocked popups.
class SafeBrowsingTriggeredPopupBlocker
    : public content::WebContentsObserver,
      public subresource_filter::SubresourceFilterObserver {
 public:
  explicit SafeBrowsingTriggeredPopupBlocker(
      content::WebContents* web_contents);
  ~SafeBrowsingTriggeredPopupBlocker() override;

  bool ShouldApplyStrongPopupBlocker(
      const content::OpenURLParams* open_url_params);

 private:
  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // subresource_filter::SubresourceFilterObserver:
  void OnSafeBrowsingCheckComplete(
      content::NavigationHandle* navigation_handle,
      safe_browsing::SBThreatType threat_type,
      const safe_browsing::ThreatMetadata& threat_metadata) override;
  void OnSubresourceFilterGoingAway() override;

  ScopedObserver<subresource_filter::SubresourceFilterObserverManager,
                 subresource_filter::SubresourceFilterObserver>
      scoped_observer_;

  // Whether the next main frame navigation that commits should trigger the
  // stronger popup blocker in enforce or warn mode.
  base::Optional<safe_browsing::SubresourceFilterLevel>
      level_for_next_committed_navigation_;

  // Whether to ignore the threat pattern type. Useful for flexibility because
  // we have to wait until metadata patterns reach Stable before using them
  // without error. Governed by a variation param.
  bool ignore_sublists_ = false;

  // Whether the current committed page load should trigger the stronger popup
  // blocker.
  bool is_triggered_for_current_committed_load_ = false;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingTriggeredPopupBlocker);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_SAFE_BROWSING_TRIGGERED_POPUP_BLOCKER_H_
