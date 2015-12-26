// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/websocket_host.h"

#include <utility>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/renderer_host/websocket_dispatcher_host.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/common/websocket_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "ipc/ipc_message_macros.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_info.h"
#include "net/websockets/websocket_channel.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_frame.h"  // for WebSocketFrameHeader::OpCode
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"
#include "url/origin.h"

namespace content {

namespace {

typedef net::WebSocketEventInterface::ChannelState ChannelState;

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
  static_assert(static_cast<OpCode>(WEB_SOCKET_MESSAGE_TYPE_CONTINUATION) ==
                    net::WebSocketFrameHeader::kOpCodeContinuation,
                "enum values must match for opcode continuation");
  static_assert(static_cast<OpCode>(WEB_SOCKET_MESSAGE_TYPE_TEXT) ==
                    net::WebSocketFrameHeader::kOpCodeText,
                "enum values must match for opcode text");
  static_assert(static_cast<OpCode>(WEB_SOCKET_MESSAGE_TYPE_BINARY) ==
                    net::WebSocketFrameHeader::kOpCodeBinary,
                "enum values must match for opcode binary");
  return static_cast<OpCode>(type);
}

WebSocketMessageType OpCodeToMessageType(
    net::WebSocketFrameHeader::OpCode opCode) {
  DCHECK(opCode == net::WebSocketFrameHeader::kOpCodeContinuation ||
         opCode == net::WebSocketFrameHeader::kOpCodeText ||
         opCode == net::WebSocketFrameHeader::kOpCodeBinary);
  // This cast is guaranteed valid by the static_assert() statements above.
  return static_cast<WebSocketMessageType>(opCode);
}

ChannelState StateCast(WebSocketDispatcherHost::WebSocketHostState host_state) {
  const WebSocketDispatcherHost::WebSocketHostState WEBSOCKET_HOST_ALIVE =
      WebSocketDispatcherHost::WEBSOCKET_HOST_ALIVE;
  const WebSocketDispatcherHost::WebSocketHostState WEBSOCKET_HOST_DELETED =
      WebSocketDispatcherHost::WEBSOCKET_HOST_DELETED;

  DCHECK(host_state == WEBSOCKET_HOST_ALIVE ||
         host_state == WEBSOCKET_HOST_DELETED);
  // These compile asserts verify that we can get away with using static_cast<>
  // for the conversion.
  static_assert(static_cast<ChannelState>(WEBSOCKET_HOST_ALIVE) ==
                    net::WebSocketEventInterface::CHANNEL_ALIVE,
                "enum values must match for state_alive");
  static_assert(static_cast<ChannelState>(WEBSOCKET_HOST_DELETED) ==
                    net::WebSocketEventInterface::CHANNEL_DELETED,
                "enum values must match for state_deleted");
  return static_cast<ChannelState>(host_state);
}

// Implementation of net::WebSocketEventInterface. Receives events from our
// WebSocketChannel object. Each event is translated to an IPC and sent to the
// renderer or child process via WebSocketDispatcherHost.
class WebSocketEventHandler : public net::WebSocketEventInterface {
 public:
  WebSocketEventHandler(WebSocketDispatcherHost* dispatcher,
                        int routing_id,
                        int render_frame_id);
  ~WebSocketEventHandler() override;

  // net::WebSocketEventInterface implementation

  ChannelState OnAddChannelResponse(const std::string& selected_subprotocol,
                                    const std::string& extensions) override;
  ChannelState OnDataFrame(bool fin,
                           WebSocketMessageType type,
                           const std::vector<char>& data) override;
  ChannelState OnClosingHandshake() override;
  ChannelState OnFlowControl(int64_t quota) override;
  ChannelState OnDropChannel(bool was_clean,
                             uint16_t code,
                             const std::string& reason) override;
  ChannelState OnFailChannel(const std::string& message) override;
  ChannelState OnStartOpeningHandshake(
      scoped_ptr<net::WebSocketHandshakeRequestInfo> request) override;
  ChannelState OnFinishOpeningHandshake(
      scoped_ptr<net::WebSocketHandshakeResponseInfo> response) override;
  ChannelState OnSSLCertificateError(
      scoped_ptr<net::WebSocketEventInterface::SSLErrorCallbacks> callbacks,
      const GURL& url,
      const net::SSLInfo& ssl_info,
      bool fatal) override;

 private:
  class SSLErrorHandlerDelegate : public SSLErrorHandler::Delegate {
   public:
    SSLErrorHandlerDelegate(
        scoped_ptr<net::WebSocketEventInterface::SSLErrorCallbacks> callbacks);
    ~SSLErrorHandlerDelegate() override;

    base::WeakPtr<SSLErrorHandler::Delegate> GetWeakPtr();

    // SSLErrorHandler::Delegate methods
    void CancelSSLRequest(int error, const net::SSLInfo* ssl_info) override;
    void ContinueSSLRequest() override;

   private:
    scoped_ptr<net::WebSocketEventInterface::SSLErrorCallbacks> callbacks_;
    base::WeakPtrFactory<SSLErrorHandlerDelegate> weak_ptr_factory_;

    DISALLOW_COPY_AND_ASSIGN(SSLErrorHandlerDelegate);
  };

  WebSocketDispatcherHost* const dispatcher_;
  const int routing_id_;
  const int render_frame_id_;
  scoped_ptr<SSLErrorHandlerDelegate> ssl_error_handler_delegate_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketEventHandler);
};

WebSocketEventHandler::WebSocketEventHandler(
    WebSocketDispatcherHost* dispatcher,
    int routing_id,
    int render_frame_id)
    : dispatcher_(dispatcher),
      routing_id_(routing_id),
      render_frame_id_(render_frame_id) {
}

WebSocketEventHandler::~WebSocketEventHandler() {
  DVLOG(1) << "WebSocketEventHandler destroyed routing_id=" << routing_id_;
}

ChannelState WebSocketEventHandler::OnAddChannelResponse(
    const std::string& selected_protocol,
    const std::string& extensions) {
  DVLOG(3) << "WebSocketEventHandler::OnAddChannelResponse"
           << " routing_id=" << routing_id_
           << " selected_protocol=\"" << selected_protocol << "\""
           << " extensions=\"" << extensions << "\"";

  return StateCast(dispatcher_->SendAddChannelResponse(
      routing_id_, selected_protocol, extensions));
}

ChannelState WebSocketEventHandler::OnDataFrame(
    bool fin,
    net::WebSocketFrameHeader::OpCode type,
    const std::vector<char>& data) {
  DVLOG(3) << "WebSocketEventHandler::OnDataFrame"
           << " routing_id=" << routing_id_ << " fin=" << fin
           << " type=" << type << " data is " << data.size() << " bytes";

  return StateCast(dispatcher_->SendFrame(
      routing_id_, fin, OpCodeToMessageType(type), data));
}

ChannelState WebSocketEventHandler::OnClosingHandshake() {
  DVLOG(3) << "WebSocketEventHandler::OnClosingHandshake"
           << " routing_id=" << routing_id_;

  return StateCast(dispatcher_->NotifyClosingHandshake(routing_id_));
}

ChannelState WebSocketEventHandler::OnFlowControl(int64_t quota) {
  DVLOG(3) << "WebSocketEventHandler::OnFlowControl"
           << " routing_id=" << routing_id_ << " quota=" << quota;

  return StateCast(dispatcher_->SendFlowControl(routing_id_, quota));
}

ChannelState WebSocketEventHandler::OnDropChannel(bool was_clean,
                                                  uint16_t code,
                                                  const std::string& reason) {
  DVLOG(3) << "WebSocketEventHandler::OnDropChannel"
           << " routing_id=" << routing_id_ << " was_clean=" << was_clean
           << " code=" << code << " reason=\"" << reason << "\"";

  return StateCast(
      dispatcher_->DoDropChannel(routing_id_, was_clean, code, reason));
}

ChannelState WebSocketEventHandler::OnFailChannel(const std::string& message) {
  DVLOG(3) << "WebSocketEventHandler::OnFailChannel"
           << " routing_id=" << routing_id_
           << " message=\"" << message << "\"";

  return StateCast(dispatcher_->NotifyFailure(routing_id_, message));
}

ChannelState WebSocketEventHandler::OnStartOpeningHandshake(
    scoped_ptr<net::WebSocketHandshakeRequestInfo> request) {
  bool should_send = dispatcher_->CanReadRawCookies();
  DVLOG(3) << "WebSocketEventHandler::OnStartOpeningHandshake "
           << "should_send=" << should_send;

  if (!should_send)
    return WebSocketEventInterface::CHANNEL_ALIVE;

  WebSocketHandshakeRequest request_to_pass;
  request_to_pass.url.Swap(&request->url);
  net::HttpRequestHeaders::Iterator it(request->headers);
  while (it.GetNext())
    request_to_pass.headers.push_back(std::make_pair(it.name(), it.value()));
  request_to_pass.headers_text =
      base::StringPrintf("GET %s HTTP/1.1\r\n",
                         request_to_pass.url.spec().c_str()) +
      request->headers.ToString();
  request_to_pass.request_time = request->request_time;

  return StateCast(dispatcher_->NotifyStartOpeningHandshake(routing_id_,
                                                            request_to_pass));
}

ChannelState WebSocketEventHandler::OnFinishOpeningHandshake(
    scoped_ptr<net::WebSocketHandshakeResponseInfo> response) {
  bool should_send = dispatcher_->CanReadRawCookies();
  DVLOG(3) << "WebSocketEventHandler::OnFinishOpeningHandshake "
           << "should_send=" << should_send;

  if (!should_send)
    return WebSocketEventInterface::CHANNEL_ALIVE;

  WebSocketHandshakeResponse response_to_pass;
  response_to_pass.url.Swap(&response->url);
  response_to_pass.status_code = response->status_code;
  response_to_pass.status_text.swap(response->status_text);
  void* iter = NULL;
  std::string name, value;
  while (response->headers->EnumerateHeaderLines(&iter, &name, &value))
    response_to_pass.headers.push_back(std::make_pair(name, value));
  response_to_pass.headers_text =
      net::HttpUtil::ConvertHeadersBackToHTTPResponse(
          response->headers->raw_headers());
  response_to_pass.response_time = response->response_time;

  return StateCast(dispatcher_->NotifyFinishOpeningHandshake(routing_id_,
                                                             response_to_pass));
}

ChannelState WebSocketEventHandler::OnSSLCertificateError(
    scoped_ptr<net::WebSocketEventInterface::SSLErrorCallbacks> callbacks,
    const GURL& url,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DVLOG(3) << "WebSocketEventHandler::OnSSLCertificateError"
           << " routing_id=" << routing_id_ << " url=" << url.spec()
           << " cert_status=" << ssl_info.cert_status << " fatal=" << fatal;
  ssl_error_handler_delegate_.reset(
      new SSLErrorHandlerDelegate(std::move(callbacks)));
  SSLManager::OnSSLCertificateSubresourceError(
      ssl_error_handler_delegate_->GetWeakPtr(), url,
      dispatcher_->render_process_id(), render_frame_id_, ssl_info, fatal);
  // The above method is always asynchronous.
  return WebSocketEventInterface::CHANNEL_ALIVE;
}

WebSocketEventHandler::SSLErrorHandlerDelegate::SSLErrorHandlerDelegate(
    scoped_ptr<net::WebSocketEventInterface::SSLErrorCallbacks> callbacks)
    : callbacks_(std::move(callbacks)), weak_ptr_factory_(this) {}

WebSocketEventHandler::SSLErrorHandlerDelegate::~SSLErrorHandlerDelegate() {}

base::WeakPtr<SSLErrorHandler::Delegate>
WebSocketEventHandler::SSLErrorHandlerDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebSocketEventHandler::SSLErrorHandlerDelegate::CancelSSLRequest(
    int error,
    const net::SSLInfo* ssl_info) {
  DVLOG(3) << "SSLErrorHandlerDelegate::CancelSSLRequest"
           << " error=" << error
           << " cert_status=" << (ssl_info ? ssl_info->cert_status
                                           : static_cast<net::CertStatus>(-1));
  callbacks_->CancelSSLRequest(error, ssl_info);
}

void WebSocketEventHandler::SSLErrorHandlerDelegate::ContinueSSLRequest() {
  DVLOG(3) << "SSLErrorHandlerDelegate::ContinueSSLRequest";
  callbacks_->ContinueSSLRequest();
}

}  // namespace

WebSocketHost::WebSocketHost(int routing_id,
                             WebSocketDispatcherHost* dispatcher,
                             net::URLRequestContext* url_request_context,
                             base::TimeDelta delay)
    : dispatcher_(dispatcher),
      url_request_context_(url_request_context),
      routing_id_(routing_id),
      delay_(delay),
      pending_flow_control_quota_(0),
      handshake_succeeded_(false),
      weak_ptr_factory_(this) {
  DVLOG(1) << "WebSocketHost: created routing_id=" << routing_id;
}

WebSocketHost::~WebSocketHost() {}

void WebSocketHost::GoAway() {
  OnDropChannel(false, static_cast<uint16_t>(net::kWebSocketErrorGoingAway),
                "");
}

bool WebSocketHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebSocketHost, message)
    IPC_MESSAGE_HANDLER(WebSocketHostMsg_AddChannelRequest, OnAddChannelRequest)
    IPC_MESSAGE_HANDLER(WebSocketMsg_SendFrame, OnSendFrame)
    IPC_MESSAGE_HANDLER(WebSocketMsg_FlowControl, OnFlowControl)
    IPC_MESSAGE_HANDLER(WebSocketMsg_DropChannel, OnDropChannel)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebSocketHost::OnAddChannelRequest(
    const GURL& socket_url,
    const std::vector<std::string>& requested_protocols,
    const url::Origin& origin,
    int render_frame_id) {
  DVLOG(3) << "WebSocketHost::OnAddChannelRequest"
           << " routing_id=" << routing_id_ << " socket_url=\"" << socket_url
           << "\" requested_protocols=\""
           << base::JoinString(requested_protocols, ", ") << "\" origin=\""
           << origin << "\"";

  DCHECK(!channel_);
  if (delay_ > base::TimeDelta()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&WebSocketHost::AddChannel, weak_ptr_factory_.GetWeakPtr(),
                   socket_url, requested_protocols, origin, render_frame_id),
        delay_);
  } else {
    AddChannel(socket_url, requested_protocols, origin, render_frame_id);
  }
  // |this| may have been deleted here.
}

void WebSocketHost::AddChannel(
    const GURL& socket_url,
    const std::vector<std::string>& requested_protocols,
    const url::Origin& origin,
    int render_frame_id) {
  DVLOG(3) << "WebSocketHost::AddChannel"
           << " routing_id=" << routing_id_ << " socket_url=\"" << socket_url
           << "\" requested_protocols=\""
           << base::JoinString(requested_protocols, ", ") << "\" origin=\""
           << origin << "\"";

  DCHECK(!channel_);

  scoped_ptr<net::WebSocketEventInterface> event_interface(
      new WebSocketEventHandler(dispatcher_, routing_id_, render_frame_id));
  channel_.reset(new net::WebSocketChannel(std::move(event_interface),
                                           url_request_context_));

  if (pending_flow_control_quota_ > 0) {
    // channel_->SendFlowControl(pending_flow_control_quota_) must be called
    // after channel_->SendAddChannelRequest() below.
    // We post OnFlowControl() here using |weak_ptr_factory_| instead of
    // calling SendFlowControl directly, because |this| may have been deleted
    // after channel_->SendAddChannelRequest().
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&WebSocketHost::OnFlowControl,
                              weak_ptr_factory_.GetWeakPtr(),
                              pending_flow_control_quota_));
    pending_flow_control_quota_ = 0;
  }

  channel_->SendAddChannelRequest(socket_url, requested_protocols, origin);
  // |this| may have been deleted here.
}

void WebSocketHost::OnSendFrame(bool fin,
                                WebSocketMessageType type,
                                const std::vector<char>& data) {
  DVLOG(3) << "WebSocketHost::OnSendFrame"
           << " routing_id=" << routing_id_ << " fin=" << fin
           << " type=" << type << " data is " << data.size() << " bytes";

  DCHECK(channel_);
  channel_->SendFrame(fin, MessageTypeToOpCode(type), data);
}

void WebSocketHost::OnFlowControl(int64_t quota) {
  DVLOG(3) << "WebSocketHost::OnFlowControl"
           << " routing_id=" << routing_id_ << " quota=" << quota;

  if (!channel_) {
    // WebSocketChannel is not yet created due to the delay introduced by
    // per-renderer WebSocket throttling.
    // SendFlowControl() is called after WebSocketChannel is created.
    pending_flow_control_quota_ += quota;
    return;
  }

  channel_->SendFlowControl(quota);
}

void WebSocketHost::OnDropChannel(bool was_clean,
                                  uint16_t code,
                                  const std::string& reason) {
  DVLOG(3) << "WebSocketHost::OnDropChannel"
           << " routing_id=" << routing_id_ << " was_clean=" << was_clean
           << " code=" << code << " reason=\"" << reason << "\"";

  if (!channel_) {
    // WebSocketChannel is not yet created due to the delay introduced by
    // per-renderer WebSocket throttling.
    WebSocketDispatcherHost::WebSocketHostState result =
        dispatcher_->DoDropChannel(routing_id_,
                                   false,
                                   net::kWebSocketErrorAbnormalClosure,
                                   "");
    DCHECK_EQ(WebSocketDispatcherHost::WEBSOCKET_HOST_DELETED, result);
    return;
  }

  // TODO(yhirano): Handle |was_clean| appropriately.
  channel_->StartClosingHandshake(code, reason);
}

}  // namespace content
