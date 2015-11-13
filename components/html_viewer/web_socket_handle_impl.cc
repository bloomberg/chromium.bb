// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/web_socket_handle_impl.h"

#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "components/html_viewer/blink_basic_type_converters.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/services/network/public/cpp/web_socket_read_queue.h"
#include "mojo/services/network/public/cpp/web_socket_write_queue.h"
#include "mojo/services/network/public/interfaces/web_socket_factory.mojom.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebSocketHandleClient.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"

using blink::WebSecurityOrigin;
using blink::WebSocketHandle;
using blink::WebSocketHandleClient;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

using mojo::ConvertTo;
using mojo::String;
using mojo::WebSocket;
using mojo::WebSocketReadQueue;

namespace mojo {

template<>
struct TypeConverter<WebSocket::MessageType, WebSocketHandle::MessageType> {
  static WebSocket::MessageType Convert(WebSocketHandle::MessageType type) {
    DCHECK(type == WebSocketHandle::MessageTypeContinuation ||
           type == WebSocketHandle::MessageTypeText ||
           type == WebSocketHandle::MessageTypeBinary);
    typedef WebSocket::MessageType MessageType;
    COMPILE_ASSERT(
        static_cast<MessageType>(WebSocketHandle::MessageTypeContinuation) ==
            WebSocket::MESSAGE_TYPE_CONTINUATION,
        enum_values_must_match_for_message_type);
    COMPILE_ASSERT(
        static_cast<MessageType>(WebSocketHandle::MessageTypeText) ==
            WebSocket::MESSAGE_TYPE_TEXT,
        enum_values_must_match_for_message_type);
    COMPILE_ASSERT(
        static_cast<MessageType>(WebSocketHandle::MessageTypeBinary) ==
            WebSocket::MESSAGE_TYPE_BINARY,
        enum_values_must_match_for_message_type);
    return static_cast<WebSocket::MessageType>(type);
  }
};

template<>
struct TypeConverter<WebSocketHandle::MessageType, WebSocket::MessageType> {
  static WebSocketHandle::MessageType Convert(WebSocket::MessageType type) {
    DCHECK(type == WebSocket::MESSAGE_TYPE_CONTINUATION ||
           type == WebSocket::MESSAGE_TYPE_TEXT ||
           type == WebSocket::MESSAGE_TYPE_BINARY);
    return static_cast<WebSocketHandle::MessageType>(type);
  }
};

}  // namespace mojo

namespace html_viewer {

// This class forms a bridge from the mojo WebSocketClient interface and the
// Blink WebSocketHandleClient interface.
class WebSocketClientImpl : public mojo::WebSocketClient {
 public:
  WebSocketClientImpl(WebSocketHandleImpl* handle,
                      blink::WebSocketHandleClient* client,
                      mojo::InterfaceRequest<mojo::WebSocketClient> request)
      : handle_(handle), client_(client), binding_(this, request.Pass()) {}
  ~WebSocketClientImpl() override {}

 private:
  // WebSocketClient methods:
  void DidConnect(const String& selected_subprotocol,
                  const String& extensions,
                  mojo::ScopedDataPipeConsumerHandle receive_stream) override {
    blink::WebSocketHandleClient* client = client_;
    WebSocketHandleImpl* handle = handle_;
    receive_stream_ = receive_stream.Pass();
    read_queue_.reset(new WebSocketReadQueue(receive_stream_.get()));
    client->didConnect(handle,
                       selected_subprotocol.To<WebString>(),
                       extensions.To<WebString>());
    // |handle| can be deleted here.
  }

  void DidReceiveData(bool fin,
                      WebSocket::MessageType type,
                      uint32_t num_bytes) override {
    read_queue_->Read(num_bytes,
                      base::Bind(&WebSocketClientImpl::DidReadFromReceiveStream,
                                 base::Unretained(this),
                                 fin, type, num_bytes));
  }

  void DidReceiveFlowControl(int64_t quota) override {
    client_->didReceiveFlowControl(handle_, quota);
    // |handle| can be deleted here.
  }

  void DidFail(const String& message) override {
    blink::WebSocketHandleClient* client = client_;
    WebSocketHandleImpl* handle = handle_;
    handle->Disconnect();  // deletes |this|
    client->didFail(handle, message.To<WebString>());
    // |handle| can be deleted here.
  }

  void DidClose(bool was_clean, uint16_t code, const String& reason) override {
    blink::WebSocketHandleClient* client = client_;
    WebSocketHandleImpl* handle = handle_;
    handle->Disconnect();  // deletes |this|
    client->didClose(handle, was_clean, code, reason.To<WebString>());
    // |handle| can be deleted here.
  }

  void DidReadFromReceiveStream(bool fin,
                                WebSocket::MessageType type,
                                uint32_t num_bytes,
                                const char* data) {
    client_->didReceiveData(handle_,
                            fin,
                            ConvertTo<WebSocketHandle::MessageType>(type),
                            data,
                            num_bytes);
    // |handle_| can be deleted here.
  }

  // |handle_| owns this object, so it is guaranteed to outlive us.
  WebSocketHandleImpl* handle_;
  blink::WebSocketHandleClient* client_;
  mojo::ScopedDataPipeConsumerHandle receive_stream_;
  scoped_ptr<WebSocketReadQueue> read_queue_;
  mojo::Binding<mojo::WebSocketClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketClientImpl);
};

WebSocketHandleImpl::WebSocketHandleImpl(mojo::WebSocketFactory* factory)
    : did_close_(false) {
  factory->CreateWebSocket(GetProxy(&web_socket_));
}

WebSocketHandleImpl::~WebSocketHandleImpl() {
  if (!did_close_) {
    // The connection is abruptly disconnected by the renderer without
    // closing handshake.
    web_socket_->Close(WebSocket::kAbnormalCloseCode, String());
  }
}

void WebSocketHandleImpl::connect(const WebURL& url,
                                  const WebVector<WebString>& protocols,
                                  const WebSecurityOrigin& origin,
                                  WebSocketHandleClient* client) {
  // TODO(mpcomplete): Is this the right ownership model? Or should mojo own
  // |client_|?
  mojo::WebSocketClientPtr client_ptr;
  mojo::MessagePipe pipe;
  client_ptr.Bind(
      mojo::InterfacePtrInfo<mojo::WebSocketClient>(pipe.handle0.Pass(), 0u));
  mojo::InterfaceRequest<mojo::WebSocketClient> request;
  request.Bind(pipe.handle1.Pass());
  client_.reset(new WebSocketClientImpl(this, client, request.Pass()));

  mojo::DataPipe data_pipe;
  send_stream_ = data_pipe.producer_handle.Pass();
  write_queue_.reset(new mojo::WebSocketWriteQueue(send_stream_.get()));
  web_socket_->Connect(url.string().utf8(),
                       mojo::Array<String>::From(protocols),
                       origin.toString().utf8(),
                       data_pipe.consumer_handle.Pass(), client_ptr.Pass());
}

void WebSocketHandleImpl::send(bool fin,
                               WebSocketHandle::MessageType type,
                               const char* data,
                               size_t size) {
  if (!client_)
    return;

  uint32_t size32 = static_cast<uint32_t>(size);
  write_queue_->Write(
      data, size32,
      base::Bind(&WebSocketHandleImpl::DidWriteToSendStream,
                 base::Unretained(this),
                 fin, type, size32));
}

void WebSocketHandleImpl::flowControl(int64_t quota) {
  if (!client_)
    return;

  web_socket_->FlowControl(quota);
}

void WebSocketHandleImpl::close(unsigned short code, const WebString& reason) {
  web_socket_->Close(code, reason.utf8());
}

void WebSocketHandleImpl::DidWriteToSendStream(
    bool fin,
    WebSocketHandle::MessageType type,
    uint32_t num_bytes,
    const char* data) {
  web_socket_->Send(fin, ConvertTo<WebSocket::MessageType>(type), num_bytes);
}

void WebSocketHandleImpl::Disconnect() {
  did_close_ = true;
  client_.reset();
}

}  // namespace html_viewer
