// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SUBRESOURCE_FILTER_AGENT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SUBRESOURCE_FILTER_AGENT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "content/public/renderer/render_frame_observer.h"

class GURL;

namespace blink {
class WebDocumentSubresourceFilter;
}  // namespace blink

namespace subresource_filter {

class RulesetDealer;
class DocumentSubresourceFilter;

// The renderer-side agent of the ContentSubresourceFilterDriver. There is one
// instance per RenderFrame, responsible for setting up the subresource filter
// for the ongoing provisional document load in the frame when instructed to do
// so by the driver.
class SubresourceFilterAgent : public content::RenderFrameObserver {
 public:
  // The |ruleset_dealer| must not be null and must outlive this instance. The
  // |render_frame| may be null in unittests.
  explicit SubresourceFilterAgent(content::RenderFrame* render_frame,
                                  RulesetDealer* ruleset_dealer);
  ~SubresourceFilterAgent() override;

 protected:
  // Returns the URLs of documents loaded into nested frames starting with the
  // current frame and ending with the main frame. The returned array is
  // guaranteed to have at least one element. Virtual so that it can be mocked
  // out in tests.
  virtual std::vector<GURL> GetAncestorDocumentURLs();

  // Injects the provided subresource |filter| into the DocumentLoader
  // orchestrating the most recently committed load. Virtual so that it can be
  // mocked out in tests.
  virtual void SetSubresourceFilterForCommittedLoad(
      std::unique_ptr<blink::WebDocumentSubresourceFilter> filter);

 private:
  void ActivateForProvisionalLoad(ActivationState activation_state);

  void RecordHistogramsOnLoadCommitted();
  void RecordHistogramsOnLoadFinished();

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidStartProvisionalLoad() override;
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_page_navigation) override;
  void DidFinishLoad() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Owned by the ChromeContentRendererClient and outlives us.
  RulesetDealer* ruleset_dealer_;

  ActivationState activation_state_for_provisional_load_;
  base::WeakPtr<DocumentSubresourceFilter> filter_for_last_committed_load_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterAgent);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SUBRESOURCE_FILTER_AGENT_H_
