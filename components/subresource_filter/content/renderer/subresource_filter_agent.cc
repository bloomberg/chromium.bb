// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
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

void SubresourceFilterAgent::OnDestruct() {
  delete this;
}

void SubresourceFilterAgent::DidStartProvisionalLoad() {
  activation_state_for_provisional_load_ = ActivationState::DISABLED;
}

void SubresourceFilterAgent::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_page_navigation) {
  if (activation_state_for_provisional_load_ != ActivationState::DISABLED &&
      ruleset_dealer_->ruleset()) {
    std::vector<GURL> ancestor_document_urls = GetAncestorDocumentURLs();
    std::unique_ptr<DocumentSubresourceFilter> filter(
        new DocumentSubresourceFilter(activation_state_for_provisional_load_,
                                      ruleset_dealer_->ruleset(),
                                      std::move(ancestor_document_urls)));
    SetSubresourceFilterForCommittedLoad(std::move(filter));
  }
  activation_state_for_provisional_load_ = ActivationState::DISABLED;
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
