// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/web_socket_impl.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/common/handle_watcher.h"
#include "mojo/services/network/network_context.h"
#include "mojo/services/public/cpp/network/web_socket_read_queue.h"
#include "mojo/services/public/cpp/network/web_socket_write_queue.h"
#include "net/websockets/websocket_channel.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_frame.h"  // for WebSocketFrameHeader::OpCode
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"
#include "url/origin.h"

namespace mojo {

template <>
struct TypeConverter<net::WebSocketFrameHeader::OpCode,
                     WebSocket::MessageType> {
  static net::WebSocketFrameHeader::OpCode Convert(
      WebSocket::MessageType type) {
    DCHECK(type == WebSocket::MESSAGE_TYPE_CONTINUATION ||
           type == WebSocket::MESSAGE_TYPE_TEXT ||
           type == WebSocket::MESSAGE_TYPE_BINARY);
    typedef net::WebSocketFrameHeader::OpCode OpCode;
    // These compile asserts verify that the same underlying values are used for
    // both types, so we can simply cast between them.
    COMPILE_ASSERT(static_cast<OpCode>(WebSocket::MESSAGE_TYPE_CONTINUATION) ==
                       net::WebSocketFrameHeader::kOpCodeContinuation,
                   enum_values_must_match_for_opcode_continuation);
    COMPILE_ASSERT(static_cast<OpCode>(WebSocket::MESSAGE_TYPE_TEXT) ==
                       net::WebSocketFrameHeader::kOpCodeText,
                   enum_values_must_match_for_opcode_text);
    COMPILE_ASSERT(static_cast<OpCode>(WebSocket::MESSAGE_TYPE_BINARY) ==
                       net::WebSocketFrameHeader::kOpCodeBinary,
                   enum_values_must_match_for_opcode_binary);
    return static_cast<OpCode>(type);
  }
};

template <>
struct TypeConverter<WebSocket::MessageType,
                     net::WebSocketFrameHeader::OpCode> {
  static WebSocket::MessageType Convert(
      net::WebSocketFrameHeader::OpCode type) {
    DCHECK(type == net::WebSocketFrameHeader::kOpCodeContinuation ||
           type == net::WebSocketFrameHeader::kOpCodeText ||
           type == net::WebSocketFrameHeader::kOpCodeBinary);
    return static_cast<WebSocket::MessageType>(type);
  }
};

namespace {

typedef net::WebSocketEventInterface::ChannelState ChannelState;

struct WebSocketEventHandler : public net::WebSocketEventInterface {
 public:
  WebSocketEventHandler(WebSocketClientPtr client)
      : client_(client.Pass()) {
  }
  virtual ~WebSocketEventHandler() {}

 private:
  // net::WebSocketEventInterface methods:
  virtual ChannelState OnAddChannelResponse(
      bool fail,
      const std::string& selected_subprotocol,
      const std::string& extensions) OVERRIDE;
  virtual ChannelState OnDataFrame(bool fin,
                                   WebSocketMessageType type,
                                   const std::vector<char>& data) OVERRIDE;
  virtual ChannelState OnClosingHandshake() OVERRIDE;
  virtual ChannelState OnFlowControl(int64 quota) OVERRIDE;
  virtual ChannelState OnDropChannel(bool was_clean,
                                     uint16 code,
                                     const std::string& reason) OVERRIDE;
  virtual ChannelState OnFailChannel(const std::string& message) OVERRIDE;
  virtual ChannelState OnStartOpeningHandshake(
      scoped_ptr<net::WebSocketHandshakeRequestInfo> request) OVERRIDE;
  virtual ChannelState OnFinishOpeningHandshake(
      scoped_ptr<net::WebSocketHandshakeResponseInfo> response) OVERRIDE;
  virtual ChannelState OnSSLCertificateError(
      scoped_ptr<net::WebSocketEventInterface::SSLErrorCallbacks> callbacks,
      const GURL& url,
      const net::SSLInfo& ssl_info,
      bool fatal) OVERRIDE;

  // Called once we've written to |receive_stream_|.
  void DidWriteToReceiveStream(bool fin,
                               net::WebSocketFrameHeader::OpCode type,
                               uint32_t num_bytes,
                               const char* buffer);
  WebSocketClientPtr client_;
  ScopedDataPipeProducerHandle receive_stream_;
  scoped_ptr<WebSocketWriteQueue> write_queue_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketEventHandler);
};

ChannelState WebSocketEventHandler::OnAddChannelResponse(
    bool fail,
    const std::string& selected_protocol,
    const std::string& extensions) {
  DataPipe data_pipe;
  receive_stream_ = data_pipe.producer_handle.Pass();
  write_queue_.reset(new WebSocketWriteQueue(receive_stream_.get()));
  client_->DidConnect(
      fail, selected_protocol, extensions, data_pipe.consumer_handle.Pass());
  if (fail)
    return WebSocketEventInterface::CHANNEL_DELETED;
  return WebSocketEventInterface::CHANNEL_ALIVE;
}

ChannelState WebSocketEventHandler::OnDataFrame(
    bool fin,
    net::WebSocketFrameHeader::OpCode type,
    const std::vector<char>& data) {
  uint32_t size = static_cast<uint32_t>(data.size());
  write_queue_->Write(
      &data[0], size,
      base::Bind(&WebSocketEventHandler::DidWriteToReceiveStream,
                 base::Unretained(this),
                 fin, type, size));
  return WebSocketEventInterface::CHANNEL_ALIVE;
}

ChannelState WebSocketEventHandler::OnClosingHandshake() {
  return WebSocketEventInterface::CHANNEL_ALIVE;
}

ChannelState WebSocketEventHandler::OnFlowControl(int64 quota) {
  client_->DidReceiveFlowControl(quota);
  return WebSocketEventInterface::CHANNEL_ALIVE;
}

ChannelState WebSocketEventHandler::OnDropChannel(bool was_clean,
                                                  uint16 code,
                                                  const std::string& reason) {
  client_->DidClose(was_clean, code, reason);
  return WebSocketEventInterface::CHANNEL_DELETED;
}

ChannelState WebSocketEventHandler::OnFailChannel(const std::string& message) {
  client_->DidFail(message);
  return WebSocketEventInterface::CHANNEL_DELETED;
}

ChannelState WebSocketEventHandler::OnStartOpeningHandshake(
    scoped_ptr<net::WebSocketHandshakeRequestInfo> request) {
  return WebSocketEventInterface::CHANNEL_ALIVE;
}

ChannelState WebSocketEventHandler::OnFinishOpeningHandshake(
    scoped_ptr<net::WebSocketHandshakeResponseInfo> response) {
  return WebSocketEventInterface::CHANNEL_ALIVE;
}

ChannelState WebSocketEventHandler::OnSSLCertificateError(
    scoped_ptr<net::WebSocketEventInterface::SSLErrorCallbacks> callbacks,
    const GURL& url,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  client_->DidFail("SSL Error");
  return WebSocketEventInterface::CHANNEL_DELETED;
}

void WebSocketEventHandler::DidWriteToReceiveStream(
    bool fin,
    net::WebSocketFrameHeader::OpCode type,
    uint32_t num_bytes,
    const char* buffer) {
  client_->DidReceiveData(
      fin, ConvertTo<WebSocket::MessageType>(type), num_bytes);
}

}  // namespace mojo

WebSocketImpl::WebSocketImpl(NetworkContext* context) : context_(context) {
}

WebSocketImpl::~WebSocketImpl() {
}

void WebSocketImpl::Connect(const String& url,
                            Array<String> protocols,
                            const String& origin,
                            ScopedDataPipeConsumerHandle send_stream,
                            WebSocketClientPtr client) {
  DCHECK(!channel_);
  send_stream_ = send_stream.Pass();
  read_queue_.reset(new WebSocketReadQueue(send_stream_.get()));
  scoped_ptr<net::WebSocketEventInterface> event_interface(
      new WebSocketEventHandler(client.Pass()));
  channel_.reset(new net::WebSocketChannel(event_interface.Pass(),
                                           context_->url_request_context()));
  channel_->SendAddChannelRequest(GURL(url.get()),
                                  protocols.To<std::vector<std::string> >(),
                                  url::Origin(origin.get()));
}

void WebSocketImpl::Send(bool fin,
                         WebSocket::MessageType type,
                         uint32_t num_bytes) {
  DCHECK(channel_);
  read_queue_->Read(num_bytes,
                    base::Bind(&WebSocketImpl::DidReadFromSendStream,
                               base::Unretained(this),
                               fin, type, num_bytes));
}

void WebSocketImpl::FlowControl(int64_t quota) {
  DCHECK(channel_);
  channel_->SendFlowControl(quota);
}

void WebSocketImpl::Close(uint16_t code, const String& reason) {
  DCHECK(channel_);
  channel_->StartClosingHandshake(code, reason);
}

void WebSocketImpl::DidReadFromSendStream(bool fin,
                                          WebSocket::MessageType type,
                                          uint32_t num_bytes,
                                          const char* data) {
  std::vector<char> buffer(num_bytes);
  memcpy(&buffer[0], data, num_bytes);
  DCHECK(channel_);
  channel_->SendFrame(
      fin, ConvertTo<net::WebSocketFrameHeader::OpCode>(type), buffer);
}

}  // namespace mojo
