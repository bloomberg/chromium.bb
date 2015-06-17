// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/devtools_agent_impl.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDevToolsAgent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace html_viewer {

DevToolsAgentImpl::DevToolsAgentImpl(blink::WebLocalFrame* frame,
                                     mojo::Shell* shell)
    : frame_(frame), binding_(this), handling_page_navigate_request_(false) {
  DCHECK(frame);
  DCHECK(shell);

  mojo::ServiceProviderPtr devtools_service_provider;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = "mojo:devtools_service";
  shell->ConnectToApplication(
      request.Pass(), mojo::GetProxy(&devtools_service_provider), nullptr);
  devtools_service::DevToolsRegistryPtr devtools_registry;
  mojo::ConnectToService(devtools_service_provider.get(), &devtools_registry);

  devtools_service::DevToolsAgentPtr agent;
  binding_.Bind(&agent);
  devtools_registry->RegisterAgent(agent.Pass());

  frame_->setDevToolsAgentClient(this);
}

DevToolsAgentImpl::~DevToolsAgentImpl() {
  if (client_)
    frame_->devToolsAgent()->detach();
}

void DevToolsAgentImpl::SetClient(
    devtools_service::DevToolsAgentClientPtr client,
    const mojo::String& client_id) {
  if (client_)
    frame_->devToolsAgent()->detach();

  client_ = client.Pass();
  client_.set_error_handler(this);

  frame_->devToolsAgent()->attach(blink::WebString::fromUTF8(client_id));
}

void DevToolsAgentImpl::DispatchProtocolMessage(const mojo::String& message) {
  // TODO(yzshen): (1) Eventually the handling code for Page.navigate (and some
  // other commands) should live with the code managing tabs and navigation.
  // We will need a DevToolsAgent implementation there as well, which handles
  // some of the commands and relays messages between the DevTools service and
  // the HTML viewer.
  // (2) Consider refactoring and reusing the existing DevTools protocol parsing
  // code.
  do {
    scoped_ptr<base::Value> value = base::JSONReader::Read(message.get());
    base::DictionaryValue* command = nullptr;
    if (!value || !value->GetAsDictionary(&command))
      break;

    std::string method;
    if (!command->GetString("method", &method) || method != "Page.navigate")
      break;

    std::string url_string;
    if (!command->GetString("params.url", &url_string))
      break;

    GURL url(url_string);
    if (!url.is_valid())
      break;

    handling_page_navigate_request_ = true;
    frame_->loadRequest(blink::WebURLRequest(url));
    handling_page_navigate_request_ = false;

    // The command should fall through to be handled by frame_->devToolsAgent().
  } while (false);

  frame_->devToolsAgent()->dispatchOnInspectorBackend(
      blink::WebString::fromUTF8(message));
}

void DevToolsAgentImpl::sendProtocolMessage(int call_id,
                                            const blink::WebString& response,
                                            const blink::WebString& state) {
  if (client_)
    client_->DispatchProtocolMessage(response.utf8());
}

void DevToolsAgentImpl::OnConnectionError() {
  client_.reset();
  frame_->devToolsAgent()->detach();
}

}  // namespace html_viewer
