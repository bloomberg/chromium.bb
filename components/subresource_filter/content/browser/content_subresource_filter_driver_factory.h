// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/safe_browsing_db/util.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace safe_browsing {
class SafeBrowsingServiceTest;
};

namespace subresource_filter {

class SubresourceFilterClient;
enum class ActivationLevel;
enum class ActivationList;

using URLToActivationListsMap =
    std::unordered_map<std::string, std::set<ActivationList>>;

// Controls the activation of subresource filtering for each page load in a
// WebContents and is responsible for sending the activation signal to all the
// per-frame SubresourceFilterAgents on the renderer side.
class ContentSubresourceFilterDriverFactory
    : public content::WebContentsUserData<
          ContentSubresourceFilterDriverFactory>,
      public content::WebContentsObserver,
      public ContentSubresourceFilterThrottleManager::Delegate {
 public:
  static void CreateForWebContents(content::WebContents* web_contents,
                                   SubresourceFilterClient* client);

  // Whether the |url|, |referrer|, and |transition| are considered to be
  // associated with a page reload.
  static bool NavigationIsPageReload(const GURL& url,
                                     const content::Referrer& referrer,
                                     ui::PageTransition transition);

  explicit ContentSubresourceFilterDriverFactory(
      content::WebContents* web_contents,
      SubresourceFilterClient* client);
  ~ContentSubresourceFilterDriverFactory() override;

  // Called when Safe Browsing detects that the |url| corresponding to the load
  // of the main frame belongs to the blacklist with |threat_type|. If the
  // blacklist is the Safe Browsing Social Engineering ads landing, then |url|
  // and |redirects| are saved.
  void OnMainResourceMatchedSafeBrowsingBlacklist(
      const GURL& url,
      safe_browsing::SBThreatType threat_type,
      safe_browsing::ThreatPatternType threat_type_metadata);

  // Reloads the page and inserts the host of its URL to the whitelist.
  void OnReloadRequested();

  // Returns the |ActivationDecision| for the current main frame
  // document.
  ActivationDecision GetActivationDecisionForLastCommittedPageLoad() const {
    return activation_decision_;
  }

  // ContentSubresourceFilterThrottleManager::Delegate:
  void OnFirstSubresourceLoadDisallowed() override;
  void WillProcessResponse(
      content::NavigationHandle* navigation_handle) override;

  ContentSubresourceFilterThrottleManager* throttle_manager() {
    return throttle_manager_.get();
  }

  SubresourceFilterClient* client() { return client_; }

 private:
  friend class ContentSubresourceFilterDriverFactoryTest;
  friend class safe_browsing::SafeBrowsingServiceTest;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Checks base on the value of |url| and current activation scope if
  // activation signal should be sent.
  void ComputeActivationForMainFrameNavigation(
      content::NavigationHandle* navigation_handle);

  // Returns whether a main-frame navigation to the given |url| satisfies the
  // activation |conditions| of a given configuration, except for |priority|.
  bool DoesMainFrameURLSatisfyActivationConditions(
      const GURL& url,
      const Configuration::ActivationConditions& conditions) const;

  bool DidURLMatchActivationList(const GURL& url,
                                 ActivationList activation_list) const;

  void AddActivationListMatch(const GURL& url, ActivationList match_type);

  // Must outlive this class.
  SubresourceFilterClient* client_;

  std::unique_ptr<ContentSubresourceFilterThrottleManager> throttle_manager_;

  // The activation decision corresponding to the most recently _started_
  // non-same-document navigation in the main frame.
  //
  // The value is reset to ActivationDecision::UNKNOWN at the start of each such
  // navigation, and will not be assigned until the navigation successfully
  // reaches the WillProcessResponse stage (or successfully finishes if
  // throttles are not invoked). This means that after a cancelled or otherwise
  // unsuccessful navigation, the value will be left at UNKNOWN indefinitely.
  ActivationDecision activation_decision_ =
      ActivationDecision::ACTIVATION_DISABLED;

  // The activation options corresponding to the most recently _committed_
  // non-same-document navigation in the main frame.
  //
  // The value corresponding to the previous such navigation will be retained,
  // and the new value not assigned until a subsequent navigation successfully
  // reaches the WillProcessResponse stage (or successfully finishes if
  // throttles are not invoked).
  Configuration::ActivationOptions activation_options_;

  URLToActivationListsMap activation_list_matches_;

  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriverFactory);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_
