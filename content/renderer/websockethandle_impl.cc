// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/websockethandle_impl.h"

#include <stdint.h>
#include <string.h>

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "third_party/WebKit/public/platform/InterfaceProvider.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/websockets/WebSocketHandle.h"
#include "third_party/WebKit/public/platform/modules/websockets/WebSocketHandleClient.h"
#include "third_party/WebKit/public/platform/modules/websockets/WebSocketHandshakeRequestInfo.h"
#include "third_party/WebKit/public/platform/modules/websockets/WebSocketHandshakeResponseInfo.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::WebSecurityOrigin;
using blink::WebSocketHandle;
using blink::WebSocketHandleClient;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {
namespace {

const uint16_t kAbnormalShutdownOpCode = 1006;

}  // namespace

WebSocketHandleImpl::WebSocketHandleImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : client_(nullptr),
      client_binding_(this),
      task_runner_(std::move(task_runner)),
      did_initialize_(false) {
  DVLOG(1) << "WebSocketHandleImpl @" << reinterpret_cast<void*>(this)
           << " created";
}

void WebSocketHandleImpl::Initialize(
    blink::InterfaceProvider* interface_provider) {
  DCHECK(!websocket_);
  DCHECK(!did_initialize_);

  interface_provider->getInterface(mojo::GetProxy(&websocket_));

  websocket_.set_connection_error_handler(
      base::Bind(&WebSocketHandleImpl::OnConnectionError,
                 base::Unretained(this)));
  did_initialize_ = true;
}

void WebSocketHandleImpl::connect(const WebURL& url,
                                  const WebVector<WebString>& protocols,
                                  const WebSecurityOrigin& origin,
                                  const WebURL& first_party_for_cookies,
                                  const WebString& user_agent_override,
                                  WebSocketHandleClient* client) {
  DVLOG(1) << "WebSocketHandleImpl @" << reinterpret_cast<void*>(this)
           << " Connect(" << url.string().utf8() << ", "
           << origin.toString().utf8() << ")";

  // It is insufficient to test if websocket_ is non-null as Disconnect() sets
  // websocket_ to null.
  if (!did_initialize_)
    Initialize(blink::Platform::current()->interfaceProvider());

  DCHECK(websocket_);

  DCHECK(!client_);
  DCHECK(client);
  client_ = client;

  std::vector<std::string> protocols_to_pass(protocols.size());
  for (size_t i = 0; i < protocols.size(); ++i)
    protocols_to_pass[i] = protocols[i].utf8();

  websocket_->AddChannelRequest(
      url, protocols_to_pass, origin, first_party_for_cookies,
      user_agent_override.latin1(),
      client_binding_.CreateInterfacePtrAndBind(task_runner_));
}

void WebSocketHandleImpl::send(bool fin,
                               WebSocketHandle::MessageType type,
                               const char* data,
                               size_t size) {
  DCHECK(websocket_);

  mojom::WebSocketMessageType type_to_pass;
  switch (type) {
    case WebSocketHandle::MessageTypeContinuation:
      type_to_pass = mojom::WebSocketMessageType::CONTINUATION;
      break;
    case WebSocketHandle::MessageTypeText:
      type_to_pass = mojom::WebSocketMessageType::TEXT;
      break;
    case WebSocketHandle::MessageTypeBinary:
      type_to_pass = mojom::WebSocketMessageType::BINARY;
      break;
    default:
      NOTREACHED();
      return;
  }

  DVLOG(1) << "WebSocketHandleImpl @" << reinterpret_cast<void*>(this)
           << " Send(" << fin << ", " << type_to_pass << ", "
           << "(data size = "  << size << "))";

  std::vector<uint8_t> data_to_pass(size);
  std::copy(data, data + size, data_to_pass.begin());

  websocket_->SendFrame(fin, type_to_pass, data_to_pass);
}

void WebSocketHandleImpl::flowControl(int64_t quota) {
  DCHECK(websocket_);

  DVLOG(1) << "WebSocketHandleImpl @" << reinterpret_cast<void*>(this)
           << " FlowControl(" << quota << ")";

  websocket_->SendFlowControl(quota);
}

void WebSocketHandleImpl::close(unsigned short code, const WebString& reason) {
  DCHECK(websocket_);

  std::string reason_to_pass = reason.utf8();

  DVLOG(1) << "WebSocketHandleImpl @" << reinterpret_cast<void*>(this)
           << " Close(" << code << ", " << reason_to_pass << ")";

  websocket_->StartClosingHandshake(code, reason_to_pass);
}

WebSocketHandleImpl::~WebSocketHandleImpl() {
  DVLOG(1) << "WebSocketHandleImpl @" << reinterpret_cast<void*>(this)
           << " deleted";

  if (websocket_)
    websocket_->StartClosingHandshake(kAbnormalShutdownOpCode, std::string());
}

void WebSocketHandleImpl::Disconnect() {
  websocket_.reset();
  client_ = nullptr;
}

void WebSocketHandleImpl::OnConnectionError() {
  if (!blink::Platform::current()) {
    // In the renderrer shutdown sequence, mojo channels are destructed and this
    // function is called. On the other hand, blink objects became invalid
    // *silently*, which means we must not touch |*client_| any more.
    // TODO(yhirano): Remove this code once the shutdown sequence is fixed.
    Disconnect();
    return;
  }

  // Our connection to the WebSocket was dropped. This could be due to
  // exceeding the maximum number of concurrent websockets from this process.

  // TODO(darin): This error message is overly specific. We don't know for sure
  // that this is the only reason we'd get here. This should be more generic or
  // we should figure out how to make it more specific.
  OnFailChannel("Error in connection establishment: "
                "net::ERR_INSUFFICIENT_RESOURCES");
}

void WebSocketHandleImpl::OnFailChannel(const std::string& message) {
  DVLOG(1) << "WebSocketHandleImpl @" << reinterpret_cast<void*>(this)
           << " OnFailChannel(" << message << ")";

  WebSocketHandleClient* client = client_;
  Disconnect();
  if (!client)
    return;

  client->didFail(this, WebString::fromUTF8(message));
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnStartOpeningHandshake(
    mojom::WebSocketHandshakeRequestPtr request) {
  DVLOG(1) << "WebSocketHandleImpl @" << reinterpret_cast<void*>(this)
           << " OnStartOpeningHandshake(" << request->url << ")";
  // All strings are already encoded to ASCII in the browser.
  blink::WebSocketHandshakeRequestInfo request_to_pass;
  request_to_pass.setURL(WebURL(request->url));
  for (size_t i = 0; i < request->headers.size(); ++i) {
    const mojom::HttpHeaderPtr& header = request->headers[i];
    request_to_pass.addHeaderField(WebString::fromLatin1(header->name),
                                   WebString::fromLatin1(header->value));
  }
  request_to_pass.setHeadersText(WebString::fromLatin1(request->headers_text));
  client_->didStartOpeningHandshake(this, request_to_pass);
}

void WebSocketHandleImpl::OnFinishOpeningHandshake(
    mojom::WebSocketHandshakeResponsePtr response) {
  DVLOG(1) << "WebSocketHandleImpl @" << reinterpret_cast<void*>(this)
           << " OnFinishOpeningHandshake(" << response->url << ")";

  // All strings are already encoded to ASCII in the browser.
  blink::WebSocketHandshakeResponseInfo response_to_pass;
  response_to_pass.setStatusCode(response->status_code);
  response_to_pass.setStatusText(WebString::fromLatin1(response->status_text));
  for (size_t i = 0; i < response->headers.size(); ++i) {
    const mojom::HttpHeaderPtr& header = response->headers[i];
    response_to_pass.addHeaderField(WebString::fromLatin1(header->name),
                                    WebString::fromLatin1(header->value));
  }
  response_to_pass.setHeadersText(
      WebString::fromLatin1(response->headers_text));
  client_->didFinishOpeningHandshake(this, response_to_pass);
}

void WebSocketHandleImpl::OnAddChannelResponse(const std::string& protocol,
                                               const std::string& extensions) {
  DVLOG(1) << "WebSocketHandleImpl @" << reinterpret_cast<void*>(this)
           << " OnAddChannelResponse("
           << protocol << ", " << extensions << ")";

  if (!client_)
    return;

  client_->didConnect(this,
                      WebString::fromUTF8(protocol),
                      WebString::fromUTF8(extensions));
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnDataFrame(bool fin,
                                      mojom::WebSocketMessageType type,
                                      const std::vector<uint8_t>& data) {
  DVLOG(1) << "WebSocketHandleImpl @" << reinterpret_cast<void*>(this)
           << " OnDataFrame(" << fin << ", " << type << ", "
           << "(data size = " << data.size() << "))";
  if (!client_)
    return;

  WebSocketHandle::MessageType type_to_pass =
      WebSocketHandle::MessageTypeContinuation;
  switch (type) {
    case mojom::WebSocketMessageType::CONTINUATION:
      type_to_pass = WebSocketHandle::MessageTypeContinuation;
      break;
    case mojom::WebSocketMessageType::TEXT:
      type_to_pass = WebSocketHandle::MessageTypeText;
      break;
    case mojom::WebSocketMessageType::BINARY:
      type_to_pass = WebSocketHandle::MessageTypeBinary;
      break;
  }
  const char* data_to_pass =
      reinterpret_cast<const char*>(data.empty() ? nullptr : &data[0]);
  client_->didReceiveData(this, fin, type_to_pass, data_to_pass, data.size());
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnFlowControl(int64_t quota) {
  DVLOG(1) << "WebSocketHandleImpl @" << reinterpret_cast<void*>(this)
           << " OnFlowControl(" << quota << ")";
  if (!client_)
    return;

  client_->didReceiveFlowControl(this, quota);
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnDropChannel(bool was_clean,
                                        uint16_t code,
                                        const std::string& reason) {
  DVLOG(1) << "WebSocketHandleImpl @" << reinterpret_cast<void*>(this)
           << " OnDropChannel(" << was_clean << ", " << code << ", "
           << reason << ")";

  WebSocketHandleClient* client = client_;
  Disconnect();
  if (!client)
    return;

  client->didClose(this, was_clean, code, WebString::fromUTF8(reason));
  // |this| can be deleted here.
}

void WebSocketHandleImpl::OnClosingHandshake() {
  DVLOG(1) << "WebSocketHandleImpl @" << reinterpret_cast<void*>(this)
           << " OnClosingHandshake()";
  if (!client_)
    return;

  client_->didStartClosingHandshake(this);
  // |this| can be deleted here.
}

}  // namespace content
