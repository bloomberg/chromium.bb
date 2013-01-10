// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_websocket_host.h"

#include <string>

#include "content/public/renderer/renderer_ppapi_host.h"
#include "net/base/net_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_websocket.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebArrayBuffer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSocket.h"

using WebKit::WebArrayBuffer;
using WebKit::WebDocument;
using WebKit::WebString;
using WebKit::WebSocket;
using WebKit::WebURL;

namespace content {

PepperWebSocketHost::PepperWebSocketHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      connecting_(false),
      initiating_close_(false),
      accepting_close_(false),
      error_was_received_(false) {
}

PepperWebSocketHost::~PepperWebSocketHost() {
  if (websocket_.get())
    websocket_->disconnect();
}

int32_t PepperWebSocketHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperWebSocketHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_WebSocket_Connect,
                                      OnHostMsgConnect)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_WebSocket_Close,
                                      OnHostMsgClose)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_WebSocket_SendText,
                                      OnHostMsgSendText)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_WebSocket_SendBinary,
                                      OnHostMsgSendBinary)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_WebSocket_Fail,
                                      OnHostMsgFail)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

void PepperWebSocketHost::didConnect() {
  std::string protocol;
  if (websocket_.get())
    protocol = websocket_->subprotocol().utf8();
  connecting_ = false;
  connect_reply_.params.set_result(PP_OK);
  host()->SendReply(connect_reply_,
                    PpapiPluginMsg_WebSocket_ConnectReply(
                        url_,
                        protocol));
}

void PepperWebSocketHost::didReceiveMessage(const WebKit::WebString& message) {
  // Dispose packets after receiving an error.
  if (error_was_received_)
    return;

  // Send an IPC to transport received data.
  std::string string_message = message.utf8();
  host()->SendUnsolicitedReply(pp_resource(),
                               PpapiPluginMsg_WebSocket_ReceiveTextReply(
                                  string_message));
}

void PepperWebSocketHost::didReceiveArrayBuffer(
    const WebKit::WebArrayBuffer& binaryData) {
  // Dispose packets after receiving an error.
  if (error_was_received_)
    return;

  // Send an IPC to transport received data.
  uint8_t* data = static_cast<uint8_t*>(binaryData.data());
  std::vector<uint8_t> array_message(data, data + binaryData.byteLength());
  host()->SendUnsolicitedReply(pp_resource(),
                               PpapiPluginMsg_WebSocket_ReceiveBinaryReply(
                                  array_message));
}

void PepperWebSocketHost::didReceiveMessageError() {
  // Records the error, then stops receiving any frames after this error.
  // The error must be notified after all queued messages are read.
  error_was_received_ = true;

  // Send an IPC to report the error. After this IPC, ReceiveTextReply and
  // ReceiveBinaryReply IPC are not sent anymore because |error_was_received_|
  // blocks.
  host()->SendUnsolicitedReply(pp_resource(),
                               PpapiPluginMsg_WebSocket_ErrorReply());
}

void PepperWebSocketHost::didUpdateBufferedAmount(
    unsigned long buffered_amount) {
  // Send an IPC to update buffered amount.
  host()->SendUnsolicitedReply(pp_resource(),
                               PpapiPluginMsg_WebSocket_BufferedAmountReply(
                                  buffered_amount));
}

void PepperWebSocketHost::didStartClosingHandshake() {
  accepting_close_ = true;

  // Send an IPC to notice that server starts closing handshake.
  host()->SendUnsolicitedReply(pp_resource(),
                               PpapiPluginMsg_WebSocket_StateReply(
                                  PP_WEBSOCKETREADYSTATE_CLOSING));
}

void PepperWebSocketHost::didClose(unsigned long unhandled_buffered_amount,
                                   ClosingHandshakeCompletionStatus status,
                                   unsigned short code,
                                   const WebKit::WebString& reason) {
  if (connecting_) {
    connecting_ = false;
    connect_reply_.params.set_result(PP_ERROR_FAILED);
    host()->SendReply(
        connect_reply_,
        PpapiPluginMsg_WebSocket_ConnectReply(url_, std::string()));
  }

  // Set close_was_clean_.
  bool was_clean =
      (initiating_close_ || accepting_close_) &&
      !unhandled_buffered_amount &&
      status == WebSocketClient::ClosingHandshakeComplete;

  if (initiating_close_) {
    initiating_close_ = false;
    close_reply_.params.set_result(PP_OK);
    host()->SendReply(close_reply_, PpapiPluginMsg_WebSocket_CloseReply(
          unhandled_buffered_amount,
          was_clean,
          code,
          reason.utf8()));
  } else {
    accepting_close_ = false;
    host()->SendUnsolicitedReply(pp_resource(),
                                 PpapiPluginMsg_WebSocket_ClosedReply(
                                     unhandled_buffered_amount,
                                     was_clean,
                                     code,
                                     reason.utf8()));
  }

  // Disconnect.
  if (websocket_.get())
    websocket_->disconnect();
}

int32_t PepperWebSocketHost::OnHostMsgConnect(
    ppapi::host::HostMessageContext* context,
    const std::string& url,
    const std::vector<std::string>& protocols) {
  // Validate url and convert it to WebURL.
  GURL gurl(url);
  url_ = gurl.spec();
  if (!gurl.is_valid())
    return PP_ERROR_BADARGUMENT;
  if (!gurl.SchemeIs("ws") && !gurl.SchemeIs("wss"))
    return PP_ERROR_BADARGUMENT;
  if (gurl.has_ref())
    return PP_ERROR_BADARGUMENT;
  if (!net::IsPortAllowedByDefault(gurl.IntPort()))
    return PP_ERROR_BADARGUMENT;
  WebURL web_url(gurl);

  // Validate protocols.
  std::string protocol_string;
  for (std::vector<std::string>::const_iterator vector_it = protocols.begin();
       vector_it != protocols.end();
       ++vector_it) {

    // Check containing characters.
    for (std::string::const_iterator string_it = vector_it->begin();
        string_it != vector_it->end();
        ++string_it) {
      uint8_t character = *string_it;
      // WebSocket specification says "(Subprotocol string must consist of)
      // characters in the range U+0021 to U+007E not including separator
      // characters as defined in [RFC2616]."
      const uint8_t minimumProtocolCharacter = '!';  // U+0021.
      const uint8_t maximumProtocolCharacter = '~';  // U+007E.
      if (character < minimumProtocolCharacter ||
          character > maximumProtocolCharacter ||
          character == '"' || character == '(' || character == ')' ||
          character == ',' || character == '/' ||
          (character >= ':' && character <= '@') ||  // U+003A - U+0040
          (character >= '[' && character <= ']') ||  // U+005B - u+005D
          character == '{' || character == '}')
        return PP_ERROR_BADARGUMENT;
    }
    // Join protocols with the comma separator.
    if (vector_it != protocols.begin())
      protocol_string.append(",");
    protocol_string.append(*vector_it);
  }

  // Convert protocols to WebString.
  WebString web_protocols = WebString::fromUTF8(protocol_string);

  // Create WebKit::WebSocket object and connect.
  WebKit::WebPluginContainer* container =
      renderer_ppapi_host_->GetContainerForInstance(pp_instance());
  if (!container)
    return PP_ERROR_BADARGUMENT;
  // TODO(toyoshim) Remove following WebDocument object copy.
  WebDocument document = container->element().document();
  websocket_.reset(WebSocket::create(document, this));
  DCHECK(websocket_.get());
  if (!websocket_.get())
    return PP_ERROR_NOTSUPPORTED;

  // Set receiving binary object type.
  websocket_->setBinaryType(WebSocket::BinaryTypeArrayBuffer);
  websocket_->connect(web_url, web_protocols);

  connect_reply_ = context->MakeReplyMessageContext();
  connecting_ = true;
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperWebSocketHost::OnHostMsgClose(
    ppapi::host::HostMessageContext* context,
    int32_t code,
    const std::string& reason) {
  if (!websocket_.get())
    return PP_ERROR_FAILED;
  close_reply_ = context->MakeReplyMessageContext();
  initiating_close_ = true;
  WebString web_reason = WebString::fromUTF8(reason);
  websocket_->close(code, web_reason);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperWebSocketHost::OnHostMsgSendText(
    ppapi::host::HostMessageContext* context,
    const std::string& message) {
  if (websocket_.get()) {
    WebString web_message = WebString::fromUTF8(message);
    websocket_->sendText(web_message);
  }
  return PP_OK;
}

int32_t PepperWebSocketHost::OnHostMsgSendBinary(
    ppapi::host::HostMessageContext* context,
    const std::vector<uint8_t>& message) {
  if (websocket_.get() && !message.empty()) {
    WebArrayBuffer web_message = WebArrayBuffer::create(message.size(), 1);
    memcpy(web_message.data(), &message.front(), message.size());
    websocket_->sendArrayBuffer(web_message);
  }
  return PP_OK;
}

int32_t PepperWebSocketHost::OnHostMsgFail(
    ppapi::host::HostMessageContext* context,
    const std::string& message) {
  if (websocket_.get())
    websocket_->fail(WebString::fromUTF8(message));
  return PP_OK;
}

}  // namespace content
