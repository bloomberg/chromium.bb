// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "components/safe_browsing_db/util.h"
#include "components/subresource_filter/core/common/document_load_statistics.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace safe_browsing {
class SafeBrowsingServiceTest;
};

namespace subresource_filter {

class SubresourceFilterClient;
enum class ActivationLevel;
enum class ActivationList;

using HostPathSet = std::set<std::string>;
using URLToActivationListsMap =
    std::unordered_map<std::string, std::set<ActivationList>>;

// Controls the activation of subresource filtering for each page load in a
// WebContents and is responsible for sending the activation signal to all the
// per-frame SubresourceFilterAgents on the renderer side.
class ContentSubresourceFilterDriverFactory
    : public base::SupportsUserData::Data,
      public content::WebContentsObserver {
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

  static void CreateForWebContents(
      content::WebContents* web_contents,
      std::unique_ptr<SubresourceFilterClient> client);
  static ContentSubresourceFilterDriverFactory* FromWebContents(
      content::WebContents* web_contents);

  // Whether the |url|, |referrer|, and |transition| are considered to be
  // associated with a page reload.
  static bool NavigationIsPageReload(const GURL& url,
                                     const content::Referrer& referrer,
                                     ui::PageTransition transition);

  explicit ContentSubresourceFilterDriverFactory(
      content::WebContents* web_contents,
      std::unique_ptr<SubresourceFilterClient> client);
  ~ContentSubresourceFilterDriverFactory() override;

  // Whitelists the host of |url|, so that page loads with the main-frame
  // document being loaded from this host will be exempted from subresource
  // filtering for the lifetime of this WebContents.
  void AddHostOfURLToWhitelistSet(const GURL& url);

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

 private:
  friend class ContentSubresourceFilterDriverFactoryTest;
  friend class safe_browsing::SafeBrowsingServiceTest;

  void ResetActivationState();

  void OnFirstSubresourceLoadDisallowed();

  void OnDocumentLoadStatistics(const DocumentLoadStatistics& statistics);

  bool IsWhitelisted(const GURL& url) const;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

  // Checks base on the value of |url| and current activation scope if
  // activation signal should be sent.
  ActivationDecision ComputeActivationDecisionForMainFrameURL(
      const GURL& url) const;
  void ActivateForFrameHostIfNeeded(content::RenderFrameHost* render_frame_host,
                                    const GURL& url);

  // Internal implementation of ReadyToCommitNavigation which doesn't use
  // NavigationHandle to ease unit tests.
  void ReadyToCommitNavigationInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      const content::Referrer& referrer,
      ui::PageTransition page_transition);

  bool DidURLMatchCurrentActivationList(const GURL& url) const;

  void AddActivationListMatch(const GURL& url, ActivationList match_type);
  void RecordRedirectChainMatchPattern() const;

  std::unique_ptr<SubresourceFilterClient> client_;

  HostPathSet whitelisted_hosts_;

  ActivationLevel activation_level_;
  ActivationDecision activation_decision_;
  bool measure_performance_;

  // The URLs in the navigation chain.
  std::vector<GURL> navigation_chain_;

  URLToActivationListsMap activation_list_matches_;

  // Statistics about subresource loads, aggregated across all frames of the
  // current page.
  DocumentLoadStatistics aggregated_document_statistics_;

  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriverFactory);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_
