// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/websockethandle_impl.h"

#include <vector>

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
struct TypeConverter<String, WebString> {
  static String Convert(const WebString& str) {
    return String(str.utf8());
  }
};
template<>
struct TypeConverter<WebString, String> {
  static WebString Convert(const String& str) {
    return WebString::fromUTF8(str.get());
  }
};

template<typename T, typename U>
struct TypeConverter<Array<T>, WebVector<U> > {
  static Array<T> Convert(const WebVector<U>& vector) {
    Array<T> array(vector.size());
    for (size_t i = 0; i < vector.size(); ++i)
      array[i] = TypeConverter<T, U>::Convert(vector[i]);
    return array.Pass();
  }
};

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
  virtual void DidConnect(
      bool fail,
      const String& selected_subprotocol,
      const String& extensions) OVERRIDE {
    client_->didConnect(handle_,
                        fail,
                        selected_subprotocol.To<WebString>(),
                        extensions.To<WebString>());
  }
  virtual void DidReceiveData(bool fin,
                              WebSocket::MessageType type,
                              ScopedDataPipeConsumerHandle data_pipe) OVERRIDE {
    uint32_t num_bytes;
    ReadDataRaw(data_pipe.get(), NULL, &num_bytes, MOJO_READ_DATA_FLAG_QUERY);
    std::vector<char> data(num_bytes);
    ReadDataRaw(
        data_pipe.get(), &data[0], &num_bytes, MOJO_READ_DATA_FLAG_NONE);
    const char* data_ptr = data.empty() ? NULL : &data[0];
    client_->didReceiveData(handle_,
                            fin,
                            ConvertTo<WebSocketHandle::MessageType>(type),
                            data_ptr,
                            data.size());
  }

  virtual void DidReceiveFlowControl(int64_t quota) OVERRIDE {
    client_->didReceiveFlowControl(handle_, quota);
    // |handle_| can be deleted here.
  }

  WebSocketHandleImpl* handle_;
  blink::WebSocketHandleClient* client_;

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
  web_socket_->Connect(url.string().utf8(),
                       Array<String>::From(protocols),
                       origin.string().utf8(),
                       client_ptr.Pass());
}

void WebSocketHandleImpl::send(bool fin,
                               WebSocketHandle::MessageType type,
                               const char* data,
                               size_t size) {
  if (!client_)
    return;

  // TODO(mpcomplete): reuse the data pipe for subsequent sends.
  uint32_t num_bytes = static_cast<uint32_t>(size);
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = num_bytes;
  DataPipe data_pipe(options);
  WriteDataRaw(data_pipe.producer_handle.get(),
               data,
               &num_bytes,
               MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  web_socket_->Send(
      fin,
      ConvertTo<WebSocket::MessageType>(type),
      data_pipe.consumer_handle.Pass());
}

void WebSocketHandleImpl::flowControl(int64_t quota) {
  if (!client_)
    return;

  web_socket_->FlowControl(quota);
}

void WebSocketHandleImpl::close(unsigned short code, const WebString& reason) {
  did_close_ = true;
  web_socket_->Close(code, reason.utf8());
}

}  // namespace mojo
