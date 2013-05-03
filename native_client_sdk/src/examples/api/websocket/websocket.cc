// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/websocket.h"

class WebSocketInstance : public pp::Instance {
 public:
  explicit WebSocketInstance(PP_Instance instance)
      : pp::Instance(instance), websocket_(NULL) {}
  virtual ~WebSocketInstance() {}
  virtual void HandleMessage(const pp::Var& var_message);

 private:
  bool IsConnected();

  void Open(const std::string& url);
  void Close();
  void Send(const std::string& message);
  void Receive();

  void OnConnectCompletion(int32_t result);
  void OnCloseCompletion(int32_t result);
  void OnReceiveCompletion(int32_t result);

  static void OnConnectCompletionCallback(void* user_data, int32_t result);
  static void OnCloseCompletionCallback(void* user_data, int32_t result);
  static void OnReceiveCompletionCallback(void* user_data, int32_t result);

  pp::WebSocket* websocket_;
  pp::Var receive_var_;
};

void WebSocketInstance::HandleMessage(const pp::Var& var_message) {
  if (!var_message.is_string())
    return;
  std::string message = var_message.AsString();
  // This message must contain a command character followed by ';' and
  // arguments like "X;arguments".
  if (message.length() < 2 || message[1] != ';')
    return;
  switch (message[0]) {
    case 'o':
      // The command 'o' requests to open the specified URL.
      // URL is passed as an argument like "o;URL".
      Open(message.substr(2));
      break;
    case 'c':
      // The command 'c' requests to close without any argument like "c;"
      Close();
      break;
    case 's':
      // The command 's' requests to send a message as a text frame. The message
      // is passed as an argument like "s;message".
      Send(message.substr(2));
      break;
  }
}

bool WebSocketInstance::IsConnected() {
  if (!websocket_)
    return false;
  if (websocket_->GetReadyState() != PP_WEBSOCKETREADYSTATE_OPEN)
    return false;
  return true;
}

void WebSocketInstance::Open(const std::string& url) {
  pp::CompletionCallback callback(OnConnectCompletionCallback, this);
  websocket_ = new pp::WebSocket(this);
  if (!websocket_)
    return;
  websocket_->Connect(pp::Var(url), NULL, 0, callback);
  PostMessage(pp::Var("connecting..."));
}

void WebSocketInstance::Close() {
  if (!IsConnected())
    return;
  pp::CompletionCallback callback(OnCloseCompletionCallback, this);
  websocket_->Close(
      PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, pp::Var("bye"), callback);
}

void WebSocketInstance::Send(const std::string& message) {
  if (!IsConnected())
    return;
  websocket_->SendMessage(pp::Var(message));
  PostMessage(pp::Var(std::string("send: ") + message));
}

void WebSocketInstance::Receive() {
  pp::CompletionCallback callback(OnReceiveCompletionCallback, this);
  // |receive_var_| must be valid until |callback| is invoked.
  // Just use a member variable.
  websocket_->ReceiveMessage(&receive_var_, callback);
}

void WebSocketInstance::OnConnectCompletion(int32_t result) {
  if (result != PP_OK) {
    PostMessage(pp::Var("connection failed"));
    return;
  }
  PostMessage(pp::Var("connected"));
  Receive();
}

void WebSocketInstance::OnCloseCompletion(int32_t result) {
  PostMessage(pp::Var(PP_OK == result ? "closed" : "abnormally closed"));
}

void WebSocketInstance::OnReceiveCompletion(int32_t result) {
  if (result == PP_OK) {
    if (receive_var_.is_array_buffer())
      PostMessage(pp::Var("receive: binary data"));
    else
      PostMessage(pp::Var(std::string("receive: ") + receive_var_.AsString()));
  }
  Receive();
}

void WebSocketInstance::OnConnectCompletionCallback(void* user_data,
                                                    int32_t result) {
  WebSocketInstance* instance = static_cast<WebSocketInstance*>(user_data);
  instance->OnConnectCompletion(result);
}

void WebSocketInstance::OnCloseCompletionCallback(void* user_data,
                                                  int32_t result) {
  WebSocketInstance* instance = static_cast<WebSocketInstance*>(user_data);
  instance->OnCloseCompletion(result);
}

void WebSocketInstance::OnReceiveCompletionCallback(void* user_data,
                                                    int32_t result) {
  WebSocketInstance* instance = static_cast<WebSocketInstance*>(user_data);
  instance->OnReceiveCompletion(result);
}

// The WebSocketModule provides an implementation of pp::Module that creates
// WebSocketInstance objects when invoked.
class WebSocketModule : public pp::Module {
 public:
  WebSocketModule() : pp::Module() {}
  virtual ~WebSocketModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new WebSocketInstance(instance);
  }
};

// Implement the required pp::CreateModule function that creates our specific
// kind of Module.
namespace pp {
Module* CreateModule() { return new WebSocketModule(); }
}  // namespace pp
