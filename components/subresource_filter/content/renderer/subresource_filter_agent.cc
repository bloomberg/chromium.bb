// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"

#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebDocumentSubresourceFilter.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace subresource_filter {

namespace {

// Placeholder subresource filter implementation that disallows all loads.
// TODO(engedy): Replace this with the actual filtering logic.
class NaysayerFilter : public blink::WebDocumentSubresourceFilter {
 public:
  NaysayerFilter() {}
  ~NaysayerFilter() override {}

  bool allowLoad(const blink::WebURL& resourceUrl,
                 blink::WebURLRequest::RequestContext) override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NaysayerFilter);
};

}  // namespace

SubresourceFilterAgent::SubresourceFilterAgent(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      activation_state_for_provisional_load_(ActivationState::DISABLED) {}

SubresourceFilterAgent::~SubresourceFilterAgent() = default;

void SubresourceFilterAgent::OnDestruct() {
  delete this;
}

void SubresourceFilterAgent::DidStartProvisionalLoad() {
  activation_state_for_provisional_load_ = ActivationState::DISABLED;
}

void SubresourceFilterAgent::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_page_navigation) {
  if (activation_state_for_provisional_load_ == ActivationState::ENABLED) {
    blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
    web_frame->dataSource()->setSubresourceFilter(new NaysayerFilter);
  }
}

bool SubresourceFilterAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SubresourceFilterAgent, message)
    IPC_MESSAGE_HANDLER(SubresourceFilterAgentMsg_ActivateForProvisionalLoad,
                        OnActivateForProvisionalLoad)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SubresourceFilterAgent::OnActivateForProvisionalLoad(
    ActivationState activation_state) {
  activation_state_for_provisional_load_ = activation_state;
}

}  // namespace subresource_filter
