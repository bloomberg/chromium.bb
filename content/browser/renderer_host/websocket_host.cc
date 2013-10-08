// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/websocket_host.h"

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "content/browser/renderer_host/websocket_dispatcher_host.h"
#include "content/common/websocket_messages.h"
#include "ipc/ipc_message_macros.h"
#include "net/websockets/websocket_channel.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_frame.h"  // for WebSocketFrameHeader::OpCode

namespace content {

namespace {

// Convert a content::WebSocketMessageType to a
// net::WebSocketFrameHeader::OpCode
net::WebSocketFrameHeader::OpCode MessageTypeToOpCode(
    WebSocketMessageType type) {
  DCHECK(type == WEB_SOCKET_MESSAGE_TYPE_CONTINUATION ||
         type == WEB_SOCKET_MESSAGE_TYPE_TEXT ||
         type == WEB_SOCKET_MESSAGE_TYPE_BINARY);
  typedef net::WebSocketFrameHeader::OpCode OpCode;
  // These compile asserts verify that the same underlying values are used for
  // both types, so we can simply cast between them.
  COMPILE_ASSERT(static_cast<OpCode>(WEB_SOCKET_MESSAGE_TYPE_CONTINUATION) ==
                     net::WebSocketFrameHeader::kOpCodeContinuation,
                 enum_values_must_match_for_opcode_continuation);
  COMPILE_ASSERT(static_cast<OpCode>(WEB_SOCKET_MESSAGE_TYPE_TEXT) ==
                     net::WebSocketFrameHeader::kOpCodeText,
                 enum_values_must_match_for_opcode_text);
  COMPILE_ASSERT(static_cast<OpCode>(WEB_SOCKET_MESSAGE_TYPE_BINARY) ==
                     net::WebSocketFrameHeader::kOpCodeBinary,
                 enum_values_must_match_for_opcode_binary);
  return static_cast<OpCode>(type);
}

WebSocketMessageType OpCodeToMessageType(
    net::WebSocketFrameHeader::OpCode opCode) {
  DCHECK(opCode == net::WebSocketFrameHeader::kOpCodeContinuation ||
         opCode == net::WebSocketFrameHeader::kOpCodeText ||
         opCode == net::WebSocketFrameHeader::kOpCodeBinary);
  // This cast is guaranteed valid by the COMPILE_ASSERT() statements above.
  return static_cast<WebSocketMessageType>(opCode);
}

// Implementation of net::WebSocketEventInterface. Receives events from our
// WebSocketChannel object. Each event is translated to an IPC and sent to the
// renderer or child process via WebSocketDispatcherHost.
class WebSocketEventHandler : public net::WebSocketEventInterface {
 public:
  WebSocketEventHandler(WebSocketDispatcherHost* dispatcher, int routing_id);
  virtual ~WebSocketEventHandler();

  // net::WebSocketEventInterface implementation

  // TODO(ricea): Add |extensions| parameter to pass the list of enabled
  // WebSocket extensions through to the renderer to make it visible to
  // Javascript.
  virtual void OnAddChannelResponse(
      bool fail,
      const std::string& selected_subprotocol) OVERRIDE;
  virtual void OnDataFrame(bool fin,
                           WebSocketMessageType type,
                           const std::vector<char>& data) OVERRIDE;
  virtual void OnClosingHandshake() OVERRIDE;
  virtual void OnFlowControl(int64 quota) OVERRIDE;
  virtual void OnDropChannel(uint16 code,
                             const std::string& reason) OVERRIDE;

 private:
  WebSocketDispatcherHost* const dispatcher_;
  const int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketEventHandler);
};

WebSocketEventHandler::WebSocketEventHandler(
    WebSocketDispatcherHost* dispatcher,
    int routing_id)
    : dispatcher_(dispatcher), routing_id_(routing_id) {}

WebSocketEventHandler::~WebSocketEventHandler() {
  DVLOG(1) << "WebSocketEventHandler destroyed routing_id= " << routing_id_;
}

void WebSocketEventHandler::OnAddChannelResponse(
    bool fail,
    const std::string& selected_protocol) {
  dispatcher_->SendAddChannelResponse(
      routing_id_, fail, selected_protocol, std::string());
  // |this| may have been deleted here.
}

void WebSocketEventHandler::OnDataFrame(bool fin,
                                        net::WebSocketFrameHeader::OpCode type,
                                        const std::vector<char>& data) {
  dispatcher_->SendFrame(routing_id_, fin, OpCodeToMessageType(type), data);
  // |this| may have been deleted here.
}

void WebSocketEventHandler::OnClosingHandshake() {
  dispatcher_->SendClosing(routing_id_);
  // |this| may have been deleted here.
}

void WebSocketEventHandler::OnFlowControl(int64 quota) {
  dispatcher_->SendFlowControl(routing_id_, quota);
  // |this| may have been deleted here.
}

void WebSocketEventHandler::OnDropChannel(uint16 code,
                                          const std::string& reason) {
  dispatcher_->DoDropChannel(routing_id_, code, reason);
  // |this| has been deleted here.
}

}  // namespace

WebSocketHost::WebSocketHost(int routing_id,
                             WebSocketDispatcherHost* dispatcher,
                             net::URLRequestContext* url_request_context) {
  DVLOG(1) << "WebSocketHost: created routing_id= " << routing_id;
  scoped_ptr<net::WebSocketEventInterface> event_interface(
      new WebSocketEventHandler(dispatcher, routing_id));
  channel_.reset(
      new net::WebSocketChannel(event_interface.Pass(), url_request_context));
}

WebSocketHost::~WebSocketHost() {}

bool WebSocketHost::OnMessageReceived(const IPC::Message& message,
                                      bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(WebSocketHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(WebSocketHostMsg_AddChannelRequest, OnAddChannelRequest)
    IPC_MESSAGE_HANDLER(WebSocketMsg_SendFrame, OnSendFrame)
    IPC_MESSAGE_HANDLER(WebSocketMsg_FlowControl, OnFlowControl)
    IPC_MESSAGE_HANDLER(WebSocketMsg_DropChannel, OnDropChannel)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void WebSocketHost::OnAddChannelRequest(
    const GURL& socket_url,
    const std::vector<std::string>& requested_protocols,
    const GURL& origin) {
  DVLOG(3) << "WebSocketHost::OnAddChannelRequest"
           << " routing_id= " << routing_id_ << " socket_url= " << socket_url
           << " requested_protocols= " << JoinString(requested_protocols, ", ")
           << " origin= " << origin;

  channel_->SendAddChannelRequest(socket_url, requested_protocols, origin);
}

void WebSocketHost::OnSendFrame(bool fin,
                                WebSocketMessageType type,
                                const std::vector<char>& data) {
  DVLOG(3) << "WebSocketHost::OnSendFrame"
           << " routing_id= " << routing_id_ << " fin= " << fin
           << " type= " << type << " data is " << data.size() << " bytes";

  channel_->SendFrame(fin, MessageTypeToOpCode(type), data);
}

void WebSocketHost::OnFlowControl(int64 quota) {
  DVLOG(3) << "WebSocketHost::OnFlowControl"
           << " routing_id= " << routing_id_ << " quota= " << quota;

  channel_->SendFlowControl(quota);
}

void WebSocketHost::OnDropChannel(uint16 code, const std::string& reason) {
  DVLOG(3) << "WebSocketDispatcherHost::OnDropChannel"
           << " routing_id= " << routing_id_ << " code= " << code
           << " reason= " << reason;

  channel_->StartClosingHandshake(code, reason);
}


}  // namespace content
