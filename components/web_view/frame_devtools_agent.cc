// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/frame_devtools_agent.h"

#include <string.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/web_view/frame_devtools_agent_delegate.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "url/gurl.h"

namespace web_view {

using devtools_service::DevToolsAgentClientPtr;
using devtools_service::DevToolsAgentPtr;
using devtools_service::DevToolsRegistryPtr;

using mojo::String;

namespace {

void StringToVector(const std::string& in, std::vector<uint8_t>* out) {
  out->resize(in.size());
  if (!in.empty())
    memcpy(&out->front(), in.c_str(), in.size());
}

}  // namespace

// This class is used by FrameDevToolsAgent to relay client messages from the
// frame to the DevTools service.
class FrameDevToolsAgent::FrameDevToolsAgentClient
    : public devtools_service::DevToolsAgentClient {
 public:
  FrameDevToolsAgentClient(FrameDevToolsAgent* owner,
                           DevToolsAgentClientPtr forward_client)
      : owner_(owner),
        binding_(this),
        forward_client_(std::move(forward_client)) {
    forward_client_.set_connection_error_handler(base::Bind(
        &FrameDevToolsAgent::OnForwardClientClosed, base::Unretained(owner_)));
    if (owner_->forward_agent_)
      OnAttachedFrame();
  }

  ~FrameDevToolsAgentClient() override {}

  void OnAttachedFrame() {
    DCHECK(owner_->forward_agent_);

    if (binding_.is_bound())
      binding_.Close();

    owner_->forward_agent_->SetClient(binding_.CreateInterfacePtrAndBind());
  }

 private:
  // DevToolsAgentClient implementation.
  void DispatchProtocolMessage(int32_t call_id,
                               const String& message,
                               const String& state) override {
    DCHECK(forward_client_);
    owner_->OnReceivedClientMessage(call_id, message, state);
    // The state is used to persist state across frame navigations. There is no
    // need to forward it.
    forward_client_->DispatchProtocolMessage(call_id, message, String());
  }

  FrameDevToolsAgent* const owner_;
  mojo::Binding<DevToolsAgentClient> binding_;
  // The DevToolsAgentClient interface provided by the DevTools service. This
  // class will forward DevToolsAgentClient method calls it receives to
  // |forward_client_|.
  DevToolsAgentClientPtr forward_client_;

  DISALLOW_COPY_AND_ASSIGN(FrameDevToolsAgentClient);
};

FrameDevToolsAgent::FrameDevToolsAgent(mojo::ApplicationImpl* app,
                                       FrameDevToolsAgentDelegate* delegate)
    : app_(app),
      delegate_(delegate),
      id_(base::GenerateGUID()),
      binding_(this) {
  DCHECK(app_);
  DCHECK(delegate_);
}

FrameDevToolsAgent::~FrameDevToolsAgent() {}

void FrameDevToolsAgent::AttachFrame(
    DevToolsAgentPtr forward_agent,
    Frame::ClientPropertyMap* devtools_properties) {
  RegisterAgentIfNecessary();
  forward_agent_ = std::move(forward_agent);

  StringToVector(id_, &(*devtools_properties)["devtools-id"]);
  if (client_impl_) {
    StringToVector(state_, &(*devtools_properties)["devtools-state"]);
    client_impl_->OnAttachedFrame();
  }

  // This follows Chrome's logic and relies on the fact that call IDs increase
  // monotonously so iterating over |pending_messages_| preserves the order in
  // which they were received.
  for (const auto& item : pending_messages_)
    forward_agent_->DispatchProtocolMessage(item.second);
}

void FrameDevToolsAgent::RegisterAgentIfNecessary() {
  if (binding_.is_bound())
    return;

  DevToolsRegistryPtr devtools_registry;
  app_->ConnectToService("mojo:devtools_service", &devtools_registry);

  devtools_registry->RegisterAgent(id_, binding_.CreateInterfacePtrAndBind());
}

void FrameDevToolsAgent::HandlePageNavigateRequest(
    const base::DictionaryValue* request) {
  std::string method;
  if (!request->GetString("method", &method) || method != "Page.navigate")
    return;

  std::string url_string;
  if (!request->GetString("params.url", &url_string))
    return;

  GURL url(url_string);
  if (!url.is_valid())
    return;

  // Stop sending messages to the old frame which will be navigated away soon.
  // However, we don't reset |client_impl_| because we want to receive responses
  // and events from the old frame in the meantime.
  forward_agent_.reset();

  delegate_->HandlePageNavigateRequest(url);
}

void FrameDevToolsAgent::SetClient(DevToolsAgentClientPtr client) {
  client_impl_.reset(new FrameDevToolsAgentClient(this, std::move(client)));
}

void FrameDevToolsAgent::DispatchProtocolMessage(const String& message) {
  // TODO(yzshen): Consider refactoring and reusing the existing DevTools
  // protocol parsing code.

  scoped_ptr<base::Value> value = base::JSONReader::Read(message.get());
  base::DictionaryValue* command = nullptr;
  if (!value || !value->GetAsDictionary(&command)) {
    VLOG(1) << "Unable to parse DevTools message: " << message;
    return;
  }

  int call_id = -1;
  if (!command->GetInteger("id", &call_id)) {
    VLOG(1) << "Call Id not found in a DevTools request message: " << message;
    return;
  }

  pending_messages_[call_id] = message;

  HandlePageNavigateRequest(command);

  if (forward_agent_)
    forward_agent_->DispatchProtocolMessage(message);
}

void FrameDevToolsAgent::OnReceivedClientMessage(int32_t call_id,
                                                 const String& message,
                                                 const String& state) {
  if (!state.is_null() && state.size() > 0)
    state_ = state;

  pending_messages_.erase(call_id);
}

void FrameDevToolsAgent::OnForwardClientClosed() {
  client_impl_.reset();
  state_.clear();
  pending_messages_.clear();
}

}  // namespace web_view
