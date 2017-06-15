// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

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

  explicit ContentSubresourceFilterDriverFactory(
      content::WebContents* web_contents,
      SubresourceFilterClient* client);
  ~ContentSubresourceFilterDriverFactory() override;

  void NotifyPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      ActivationDecision activation_decision,
      Configuration::ActivationOptions matched_options);

  // Returns the |ActivationDecision| for the current main frame document. Do
  // not rely on this API, it is only temporary.
  // TODO(csharrison): Remove this and |activation_decision_| once consumers
  // move to become SubresourceFilterObservers.
  ActivationDecision GetActivationDecisionForLastCommittedPageLoad() const {
    return activation_decision_;
  }

  // Returns the |ActivationOptions| for the current main frame
  // document. Do not rely on this API, it is only temporary.
  // TODO(csharrison): Remove this and |activation_options_| in place of adding
  // |should_suppress_notifications| on ActivationState.
  const Configuration::ActivationOptions&
  GetActivationOptionsForLastCommittedPageLoad() const {
    return activation_options_;
  }

  // ContentSubresourceFilterThrottleManager::Delegate:
  void OnFirstSubresourceLoadDisallowed() override;
  bool AllowStrongPopupBlocking() override;

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

  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriverFactory);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_
