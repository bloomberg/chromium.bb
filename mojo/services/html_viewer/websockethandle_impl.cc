// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/websockethandle_impl.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "mojo/services/html_viewer/blink_basic_type_converters.h"
#include "mojo/services/public/cpp/network/web_socket_read_queue.h"
#include "mojo/services/public/cpp/network/web_socket_write_queue.h"
#include "mojo/services/public/interfaces/network/network_service.mojom.h"
#include "third_party/WebKit/public/platform/WebSerializedOrigin.h"
#include "third_party/WebKit/public/platform/WebSocketHandleClient.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"

using blink::WebSerializedOrigin;
using blink::WebSocketHandle;
using blink::WebSocketHandleClient;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

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

// This class forms a bridge from the mojo WebSocketClient interface and the
// Blink WebSocketHandleClient interface.
class WebSocketClientImpl : public InterfaceImpl<WebSocketClient> {
 public:
  explicit WebSocketClientImpl(WebSocketHandleImpl* handle,
                               blink::WebSocketHandleClient* client)
      : handle_(handle), client_(client) {}
  virtual ~WebSocketClientImpl() {}

 private:
  // WebSocketClient methods:
  virtual void DidConnect(bool fail,
                          const String& selected_subprotocol,
                          const String& extensions,
                          ScopedDataPipeConsumerHandle receive_stream)
      OVERRIDE {
    blink::WebSocketHandleClient* client = client_;
    WebSocketHandleImpl* handle = handle_;
    receive_stream_ = receive_stream.Pass();
    read_queue_.reset(new WebSocketReadQueue(receive_stream_.get()));
    if (fail)
      handle->Disconnect();  // deletes |this|
    client->didConnect(handle,
                       fail,
                       selected_subprotocol.To<WebString>(),
                       extensions.To<WebString>());
    // |handle| can be deleted here.
  }

  virtual void DidReceiveData(bool fin,
                              WebSocket::MessageType type,
                              uint32_t num_bytes) OVERRIDE {
    read_queue_->Read(num_bytes,
                      base::Bind(&WebSocketClientImpl::DidReadFromReceiveStream,
                                 base::Unretained(this),
                                 fin, type, num_bytes));
  }

  virtual void DidReceiveFlowControl(int64_t quota) OVERRIDE {
    client_->didReceiveFlowControl(handle_, quota);
    // |handle| can be deleted here.
  }

  virtual void DidFail(const String& message) OVERRIDE {
    blink::WebSocketHandleClient* client = client_;
    WebSocketHandleImpl* handle = handle_;
    handle->Disconnect();  // deletes |this|
    client->didFail(handle, message.To<WebString>());
    // |handle| can be deleted here.
  }

  virtual void DidClose(bool was_clean,
                        uint16_t code,
                        const String& reason) OVERRIDE {
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
  ScopedDataPipeConsumerHandle receive_stream_;
  scoped_ptr<WebSocketReadQueue> read_queue_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketClientImpl);
};

WebSocketHandleImpl::WebSocketHandleImpl(NetworkService* network_service)
    : did_close_(false) {
  network_service->CreateWebSocket(Get(&web_socket_));
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
                                  const WebSerializedOrigin& origin,
                                  WebSocketHandleClient* client) {
  client_.reset(new WebSocketClientImpl(this, client));
  WebSocketClientPtr client_ptr;
  // TODO(mpcomplete): Is this the right ownership model? Or should mojo own
  // |client_|?
  WeakBindToProxy(client_.get(), &client_ptr);

  DataPipe data_pipe;
  send_stream_ = data_pipe.producer_handle.Pass();
  write_queue_.reset(new WebSocketWriteQueue(send_stream_.get()));
  web_socket_->Connect(url.string().utf8(),
                       Array<String>::From(protocols),
                       origin.string().utf8(),
                       data_pipe.consumer_handle.Pass(),
                       client_ptr.Pass());
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

}  // namespace mojo
