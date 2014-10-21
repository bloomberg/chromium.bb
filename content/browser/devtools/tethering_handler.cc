// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/tethering_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_http_handler_impl.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/socket/stream_listen_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"

namespace content {

namespace {

const char kLocalhost[] = "127.0.0.1";

const int kListenBacklog = 5;
const int kBufferSize = 16 * 1024;

const int kMinTetheringPort = 1024;
const int kMaxTetheringPort = 32767;

class SocketPump : public net::StreamListenSocket::Delegate {
 public:
  SocketPump(DevToolsHttpHandlerDelegate* delegate,
             net::StreamSocket* client_socket)
      : client_socket_(client_socket),
        delegate_(delegate),
        wire_buffer_size_(0),
        pending_destruction_(false) {
  }

  std::string Init() {
    std::string channel_name;
    server_socket_ = delegate_->CreateSocketForTethering(this, &channel_name);
    if (!server_socket_.get() || channel_name.empty())
      SelfDestruct();
    return channel_name;
  }

  ~SocketPump() override {}

 private:
  void DidAccept(net::StreamListenSocket* server,
                 scoped_ptr<net::StreamListenSocket> socket) override {
    if (accepted_socket_.get())
      return;

    buffer_ = new net::IOBuffer(kBufferSize);
    wire_buffer_ = new net::GrowableIOBuffer();
    wire_buffer_->SetCapacity(kBufferSize);

    accepted_socket_ = socket.Pass();
    int result = client_socket_->Read(
        buffer_.get(),
        kBufferSize,
        base::Bind(&SocketPump::OnClientRead, base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      OnClientRead(result);
  }

  void DidRead(net::StreamListenSocket* socket,
               const char* data,
               int len) override {
    int old_size = wire_buffer_size_;
    wire_buffer_size_ += len;
    while (wire_buffer_->capacity() < wire_buffer_size_)
      wire_buffer_->SetCapacity(wire_buffer_->capacity() * 2);
    memcpy(wire_buffer_->StartOfBuffer() + old_size, data, len);
    if (old_size != wire_buffer_->offset())
      return;
    OnClientWrite(0);
  }

  void DidClose(net::StreamListenSocket* socket) override { SelfDestruct(); }

  void OnClientRead(int result) {
    if (result <= 0) {
      SelfDestruct();
      return;
    }

    accepted_socket_->Send(buffer_->data(), result);
    result = client_socket_->Read(
        buffer_.get(),
        kBufferSize,
        base::Bind(&SocketPump::OnClientRead, base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      OnClientRead(result);
  }

  void OnClientWrite(int result) {
    if (result < 0) {
      SelfDestruct();
      return;
    }

    wire_buffer_->set_offset(wire_buffer_->offset() + result);

    int remaining = wire_buffer_size_ - wire_buffer_->offset();
    if (remaining == 0) {
      if (pending_destruction_)
        SelfDestruct();
      return;
    }


    if (remaining > kBufferSize)
      remaining = kBufferSize;

    scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(remaining);
    memcpy(buffer->data(), wire_buffer_->data(), remaining);
    result = client_socket_->Write(
        buffer.get(),
        remaining,
        base::Bind(&SocketPump::OnClientWrite, base::Unretained(this)));

    // Shrink buffer
    int offset = wire_buffer_->offset();
    if (offset > kBufferSize) {
      memcpy(wire_buffer_->StartOfBuffer(), wire_buffer_->data(),
          wire_buffer_size_ - offset);
      wire_buffer_size_ -= offset;
      wire_buffer_->set_offset(0);
    }

    if (result != net::ERR_IO_PENDING)
      OnClientWrite(result);
    return;
  }

  void SelfDestruct() {
    if (wire_buffer_.get() && wire_buffer_->offset() != wire_buffer_size_) {
      pending_destruction_ = true;
      return;
    }
    delete this;
  }

 private:
  scoped_ptr<net::StreamSocket> client_socket_;
  scoped_ptr<net::StreamListenSocket> server_socket_;
  scoped_ptr<net::StreamListenSocket> accepted_socket_;
  scoped_refptr<net::IOBuffer> buffer_;
  scoped_refptr<net::GrowableIOBuffer> wire_buffer_;
  DevToolsHttpHandlerDelegate* delegate_;
  int wire_buffer_size_;
  bool pending_destruction_;
};

static int GetPort(scoped_refptr<DevToolsProtocol::Command> command,
                   const std::string& paramName) {
  base::DictionaryValue* params = command->params();
  int port = 0;
  if (!params ||
      !params->GetInteger(paramName, &port) ||
      port < kMinTetheringPort || port > kMaxTetheringPort)
    return 0;
  return port;
}

class BoundSocket {
 public:
  typedef base::Callback<void(int, const std::string&)> AcceptedCallback;

  BoundSocket(AcceptedCallback accepted_callback,
              DevToolsHttpHandlerDelegate* delegate)
      : accepted_callback_(accepted_callback),
        delegate_(delegate),
        socket_(new net::TCPServerSocket(NULL, net::NetLog::Source())),
        port_(0) {
  }

  virtual ~BoundSocket() {
  }

  bool Listen(int port) {
    port_ = port;
    net::IPAddressNumber ip_number;
    if (!net::ParseIPLiteralToNumber(kLocalhost, &ip_number))
      return false;

    net::IPEndPoint end_point(ip_number, port);
    int result = socket_->Listen(end_point, kListenBacklog);
    if (result < 0)
      return false;

    net::IPEndPoint local_address;
    result = socket_->GetLocalAddress(&local_address);
    if (result < 0)
      return false;

    DoAccept();
    return true;
  }

 private:
  typedef std::map<net::IPEndPoint, net::StreamSocket*> AcceptedSocketsMap;

  void DoAccept() {
    while (true) {
      int result = socket_->Accept(
          &accept_socket_,
          base::Bind(&BoundSocket::OnAccepted, base::Unretained(this)));
      if (result == net::ERR_IO_PENDING)
        break;
      else
        HandleAcceptResult(result);
    }
  }

  void OnAccepted(int result) {
    HandleAcceptResult(result);
    if (result == net::OK)
      DoAccept();
  }

  void HandleAcceptResult(int result) {
    if (result != net::OK)
      return;

    SocketPump* pump = new SocketPump(delegate_, accept_socket_.release());
    std::string name = pump->Init();
    if (!name.empty())
      accepted_callback_.Run(port_, name);
  }

  AcceptedCallback accepted_callback_;
  DevToolsHttpHandlerDelegate* delegate_;
  scoped_ptr<net::ServerSocket> socket_;
  scoped_ptr<net::StreamSocket> accept_socket_;
  int port_;
};

}  // namespace

// TetheringHandler::TetheringImpl -------------------------------------------

class TetheringHandler::TetheringImpl {
 public:
  TetheringImpl(
      base::WeakPtr<TetheringHandler> handler,
      DevToolsHttpHandlerDelegate* delegate);
  ~TetheringImpl();

  void Bind(scoped_refptr<DevToolsProtocol::Command> command, int port);
  void Unbind(scoped_refptr<DevToolsProtocol::Command> command, int port);
  void Accepted(int port, const std::string& name);

 private:
  void SendInternalError(scoped_refptr<DevToolsProtocol::Command> command,
                         const std::string& message);

  base::WeakPtr<TetheringHandler> handler_;
  DevToolsHttpHandlerDelegate* delegate_;

  typedef std::map<int, BoundSocket*> BoundSockets;
  BoundSockets bound_sockets_;
};

TetheringHandler::TetheringImpl::TetheringImpl(
    base::WeakPtr<TetheringHandler> handler,
    DevToolsHttpHandlerDelegate* delegate)
    : handler_(handler),
      delegate_(delegate) {
}

TetheringHandler::TetheringImpl::~TetheringImpl() {
  STLDeleteContainerPairSecondPointers(bound_sockets_.begin(),
                                       bound_sockets_.end());
}

void TetheringHandler::TetheringImpl::Bind(
    scoped_refptr<DevToolsProtocol::Command> command, int port) {
  if (bound_sockets_.find(port) != bound_sockets_.end()) {
    SendInternalError(command, "Port already bound");
    return;
  }

  BoundSocket::AcceptedCallback callback = base::Bind(
      &TetheringHandler::TetheringImpl::Accepted, base::Unretained(this));
  scoped_ptr<BoundSocket> bound_socket(new BoundSocket(callback, delegate_));
  if (!bound_socket->Listen(port)) {
    SendInternalError(command, "Could not bind port");
    return;
  }

  bound_sockets_[port] = bound_socket.release();
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&TetheringHandler::SendBindSuccess, handler_, command));
}

void TetheringHandler::TetheringImpl::Unbind(
    scoped_refptr<DevToolsProtocol::Command> command, int port) {

  BoundSockets::iterator it = bound_sockets_.find(port);
  if (it == bound_sockets_.end()) {
    SendInternalError(command, "Port is not bound");
    return;
  }

  delete it->second;
  bound_sockets_.erase(it);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&TetheringHandler::SendUnbindSuccess, handler_, command));
}

void TetheringHandler::TetheringImpl::Accepted(
    int port, const std::string& name) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&TetheringHandler::Accepted, handler_, port, name));
}

void TetheringHandler::TetheringImpl::SendInternalError(
    scoped_refptr<DevToolsProtocol::Command> command,
    const std::string& message) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&TetheringHandler::SendInternalError, handler_,
                 command, message));
}


// TetheringHandler ----------------------------------------------------------

// static
TetheringHandler::TetheringImpl* TetheringHandler::impl_ = nullptr;

TetheringHandler::TetheringHandler(
    DevToolsHttpHandlerDelegate* delegate,
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy)
    : delegate_(delegate),
      message_loop_proxy_(message_loop_proxy),
      is_active_(false),
      weak_factory_(this) {
  RegisterCommandHandler(devtools::Tethering::bind::kName,
                         base::Bind(&TetheringHandler::OnBind,
                                    base::Unretained(this)));
  RegisterCommandHandler(devtools::Tethering::unbind::kName,
                         base::Bind(&TetheringHandler::OnUnbind,
                                    base::Unretained(this)));
}

TetheringHandler::~TetheringHandler() {
  if (is_active_) {
    message_loop_proxy_->DeleteSoon(FROM_HERE, impl_);
    impl_ = nullptr;
  }
}

void TetheringHandler::Accepted(int port, const std::string& name) {
  base::DictionaryValue* params = new base::DictionaryValue();
  params->SetInteger(devtools::Tethering::accepted::kParamPort, port);
  params->SetString(devtools::Tethering::accepted::kParamConnectionId, name);
  SendNotification(devtools::Tethering::accepted::kName, params);
}

bool TetheringHandler::Activate() {
  if (is_active_)
    return true;
  if (impl_)
    return false;
  is_active_ = true;
  impl_ = new TetheringImpl(weak_factory_.GetWeakPtr(), delegate_);
  return true;
}

scoped_refptr<DevToolsProtocol::Response>
TetheringHandler::OnBind(scoped_refptr<DevToolsProtocol::Command> command) {
  const std::string& portParamName = devtools::Tethering::bind::kParamPort;
  int port = GetPort(command, portParamName);
  if (port == 0)
    return command->InvalidParamResponse(portParamName);

  if (!Activate()) {
    return command->ServerErrorResponse(
        "Tethering is used by another connection");
  }
  DCHECK(impl_);
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&TetheringImpl::Bind, base::Unretained(impl_),
                 command, port));
  return command->AsyncResponsePromise();
}

scoped_refptr<DevToolsProtocol::Response>
TetheringHandler::OnUnbind(scoped_refptr<DevToolsProtocol::Command> command) {
  const std::string& portParamName = devtools::Tethering::unbind::kParamPort;
  int port = GetPort(command, portParamName);
  if (port == 0)
    return command->InvalidParamResponse(portParamName);

  if (!Activate()) {
    return command->ServerErrorResponse(
        "Tethering is used by another connection");
  }
  DCHECK(impl_);
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&TetheringImpl::Unbind, base::Unretained(impl_),
                 command, port));
  return command->AsyncResponsePromise();
}

void TetheringHandler::SendBindSuccess(
    scoped_refptr<DevToolsProtocol::Command> command) {
  SendAsyncResponse(command->SuccessResponse(nullptr));
}

void TetheringHandler::SendUnbindSuccess(
    scoped_refptr<DevToolsProtocol::Command> command) {
  SendAsyncResponse(command->SuccessResponse(nullptr));
}

void TetheringHandler::SendInternalError(
    scoped_refptr<DevToolsProtocol::Command> command,
    const std::string& message) {
  SendAsyncResponse(command->InternalErrorResponse(message));
}

}  // namespace content
