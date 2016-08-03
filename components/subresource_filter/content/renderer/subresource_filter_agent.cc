// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/content/renderer/document_subresource_filter.h"
#include "components/subresource_filter/content/renderer/ruleset_dealer.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"

namespace subresource_filter {

SubresourceFilterAgent::SubresourceFilterAgent(
    content::RenderFrame* render_frame,
    RulesetDealer* ruleset_dealer)
    : content::RenderFrameObserver(render_frame),
      ruleset_dealer_(ruleset_dealer),
      activation_state_for_provisional_load_(ActivationState::DISABLED) {
  DCHECK(ruleset_dealer);
}

SubresourceFilterAgent::~SubresourceFilterAgent() = default;

std::vector<GURL> SubresourceFilterAgent::GetAncestorDocumentURLs() {
  std::vector<GURL> urls;
  for (blink::WebFrame* frame = render_frame()->GetWebFrame(); frame;
       frame = frame->parent()) {
    urls.push_back(frame->document().url());
  }
  return urls;
}

void SubresourceFilterAgent::SetSubresourceFilterForCommittedLoad(
    std::unique_ptr<blink::WebDocumentSubresourceFilter> filter) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  web_frame->dataSource()->setSubresourceFilter(filter.release());
}

void SubresourceFilterAgent::ActivateForProvisionalLoad(
    ActivationState activation_state) {
  activation_state_for_provisional_load_ = activation_state;
}

void SubresourceFilterAgent::RecordHistogramsOnLoadCommitted() {
  UMA_HISTOGRAM_ENUMERATION(
      "SubresourceFilter.DocumentLoad.ActivationState",
      static_cast<int>(activation_state_for_provisional_load_),
      static_cast<int>(ActivationState::LAST) + 1);

  if (activation_state_for_provisional_load_ != ActivationState::DISABLED) {
    UMA_HISTOGRAM_BOOLEAN("SubresourceFilter.DocumentLoad.RulesetIsAvailable",
                          !!ruleset_dealer_->ruleset());
  }
}

void SubresourceFilterAgent::RecordHistogramsOnLoadFinished() {
  DCHECK(filter_for_last_committed_load_);
  UMA_HISTOGRAM_COUNTS_1000(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Total",
      filter_for_last_committed_load_->num_loads_total());
  UMA_HISTOGRAM_COUNTS_1000(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Evaluated",
      filter_for_last_committed_load_->num_loads_evaluated());
  UMA_HISTOGRAM_COUNTS_1000(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.MatchedRules",
      filter_for_last_committed_load_->num_loads_matching_rules());
  UMA_HISTOGRAM_COUNTS_1000(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Disallowed",
      filter_for_last_committed_load_->num_loads_disallowed());
}

void SubresourceFilterAgent::OnDestruct() {
  delete this;
}

void SubresourceFilterAgent::DidStartProvisionalLoad() {
  activation_state_for_provisional_load_ = ActivationState::DISABLED;
  filter_for_last_committed_load_.reset();
}

void SubresourceFilterAgent::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_page_navigation) {
  RecordHistogramsOnLoadCommitted();
  if (activation_state_for_provisional_load_ != ActivationState::DISABLED &&
      ruleset_dealer_->ruleset()) {
    std::vector<GURL> ancestor_document_urls = GetAncestorDocumentURLs();
    std::unique_ptr<DocumentSubresourceFilter> filter(
        new DocumentSubresourceFilter(activation_state_for_provisional_load_,
                                      ruleset_dealer_->ruleset(),
                                      ancestor_document_urls));
    filter_for_last_committed_load_ = filter->AsWeakPtr();
    SetSubresourceFilterForCommittedLoad(std::move(filter));
  }
  activation_state_for_provisional_load_ = ActivationState::DISABLED;
}

void SubresourceFilterAgent::DidFinishLoad() {
  if (!filter_for_last_committed_load_)
    return;

  RecordHistogramsOnLoadFinished();
}

bool SubresourceFilterAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SubresourceFilterAgent, message)
    IPC_MESSAGE_HANDLER(SubresourceFilterMsg_ActivateForProvisionalLoad,
                        ActivateForProvisionalLoad)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace subresource_filter
