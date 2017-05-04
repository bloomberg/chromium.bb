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
  // NOTE: ActivationDecision backs a UMA histogram, so it is append-only.
  enum class ActivationDecision {
    // The activation decision is unknown, or not known yet.
    UNKNOWN,

    // Subresource filtering was activated.
    ACTIVATED,

    // Did not activate because subresource filtering was disabled.
    ACTIVATION_DISABLED,

    // Did not activate because the main frame document URL had an unsupported
    // scheme.
    UNSUPPORTED_SCHEME,

    // Did not activate because the main frame document URL was whitelisted.
    URL_WHITELISTED,

    // Did not activate because the main frame document URL did not match the
    // activation list.
    ACTIVATION_LIST_NOT_MATCHED,

    // Max value for enum.
    ACTIVATION_DECISION_MAX
  };

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
      const std::vector<GURL>& redirect_urls,
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
  bool ShouldSuppressActivation(
      content::NavigationHandle* navigation_handle) override;
  void WillProcessResponse(
      content::NavigationHandle* navigation_handle) override;

  ContentSubresourceFilterThrottleManager* throttle_manager() {
    return throttle_manager_.get();
  }

  SubresourceFilterClient* client() { return client_; }

 private:
  friend class ContentSubresourceFilterDriverFactoryTest;
  friend class safe_browsing::SafeBrowsingServiceTest;

  void ResetActivationState();

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Checks base on the value of |url| and current activation scope if
  // activation signal should be sent.
  ActivationDecision ComputeActivationDecisionForMainFrameNavigation(
      content::NavigationHandle* navigation_handle) const;

  bool DidURLMatchActivationList(const GURL& url,
                                 ActivationList activation_list) const;

  void AddActivationListMatch(const GURL& url, ActivationList match_type);
  int CalculateHitPatternForActivationList(
      ActivationList activation_list) const;
  void RecordRedirectChainMatchPattern() const;

  void RecordRedirectChainMatchPatternForList(
      ActivationList activation_list) const;

  // Must outlive this class.
  SubresourceFilterClient* client_;

  std::unique_ptr<ContentSubresourceFilterThrottleManager> throttle_manager_;

  ActivationLevel activation_level_;
  ActivationDecision activation_decision_;
  bool measure_performance_;

  // The URLs in the navigation chain.
  std::vector<GURL> navigation_chain_;

  URLToActivationListsMap activation_list_matches_;

  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriverFactory);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_
