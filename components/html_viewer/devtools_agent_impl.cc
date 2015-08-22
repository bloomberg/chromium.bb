// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/devtools_agent_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDevToolsAgent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace html_viewer {

DevToolsAgentImpl::DevToolsAgentImpl(blink::WebLocalFrame* frame,
                                     const std::string& id,
                                     const std::string* state)
    : frame_(frame), id_(id), binding_(this), cache_until_client_ready_(false) {
  DCHECK(frame);
  frame_->setDevToolsAgentClient(this);

  if (state) {
    cache_until_client_ready_ = true;
    frame_->devToolsAgent()->reattach(blink::WebString::fromUTF8(id_),
                                      blink::WebString::fromUTF8(*state));
  }
}

DevToolsAgentImpl::~DevToolsAgentImpl() {
  if (cache_until_client_ready_ || client_)
    frame_->devToolsAgent()->detach();
}

void DevToolsAgentImpl::BindToRequest(
    mojo::InterfaceRequest<DevToolsAgent> request) {
  binding_.Bind(request.Pass());
}

void DevToolsAgentImpl::SetClient(
    devtools_service::DevToolsAgentClientPtr client) {
  if (client_)
    frame_->devToolsAgent()->detach();

  client_ = client.Pass();
  client_.set_connection_error_handler(base::Bind(
      &DevToolsAgentImpl::OnConnectionError, base::Unretained(this)));

  if (cache_until_client_ready_) {
    cache_until_client_ready_ = false;
    for (const auto& message : cached_client_messages_)
      client_->DispatchProtocolMessage(message.call_id, message.response,
                                       message.state);
    cached_client_messages_.clear();
  } else {
    frame_->devToolsAgent()->attach(blink::WebString::fromUTF8(id_));
  }
}

void DevToolsAgentImpl::DispatchProtocolMessage(const mojo::String& message) {
  frame_->devToolsAgent()->dispatchOnInspectorBackend(
      blink::WebString::fromUTF8(message));
}

void DevToolsAgentImpl::sendProtocolMessage(int call_id,
                                            const blink::WebString& response,
                                            const blink::WebString& state) {
  DCHECK(!response.isNull());

  mojo::String response_str = response.utf8();
  mojo::String state_str;
  if (!state.isNull())
    state_str = state.utf8();

  if (client_) {
    client_->DispatchProtocolMessage(call_id, response_str, state_str);
  } else if (cache_until_client_ready_) {
    cached_client_messages_.resize(cached_client_messages_.size() + 1);
    CachedClientMessage& message = cached_client_messages_.back();
    message.call_id = call_id;
    message.response.Swap(&response_str);
    message.state.Swap(&state_str);
  }
}

void DevToolsAgentImpl::OnConnectionError() {
  client_.reset();
  frame_->devToolsAgent()->detach();
}

}  // namespace html_viewer
