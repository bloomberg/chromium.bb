// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/socket_api.h"

#include "base/bind.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/permissions/socket_permission.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/dns/host_resolver_wrapper.h"
#include "chrome/browser/extensions/api/socket/socket.h"
#include "chrome/browser/extensions/api/socket/tcp_socket.h"
#include "chrome/browser/extensions/api/socket/udp_socket.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/io_thread.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"

namespace extensions {

using content::SocketPermissionRequest;

const char kAddressKey[] = "address";
const char kPortKey[] = "port";
const char kBytesWrittenKey[] = "bytesWritten";
const char kDataKey[] = "data";
const char kResultCodeKey[] = "resultCode";
const char kSocketIdKey[] = "socketId";

const char kSocketNotFoundError[] = "Socket not found";
const char kDnsLookupFailedError[] = "DNS resolution failed";
const char kPermissionError[] = "App does not have permission";
const char kNetworkListError[] = "Network lookup failed or unsupported";
const char kTCPSocketBindError[] =
    "TCP socket does not support bind. For TCP server please use listen.";

SocketAsyncApiFunction::SocketAsyncApiFunction()
    : manager_(NULL) {
}

SocketAsyncApiFunction::~SocketAsyncApiFunction() {
}

bool SocketAsyncApiFunction::PrePrepare() {
  manager_ = ExtensionSystem::Get(profile())->socket_manager();
  DCHECK(manager_) << "There is no socket manager. "
    "If this assertion is failing during a test, then it is likely that "
    "TestExtensionSystem is failing to provide an instance of "
    "ApiResourceManager<Socket>.";
  return manager_ != NULL;
}

bool SocketAsyncApiFunction::Respond() {
  return error_.empty();
}

Socket* SocketAsyncApiFunction::GetSocket(int api_resource_id) {
  return manager_->Get(extension_->id(), api_resource_id);
}

void SocketAsyncApiFunction::RemoveSocket(int api_resource_id) {
  manager_->Remove(extension_->id(), api_resource_id);
}

SocketExtensionWithDnsLookupFunction::SocketExtensionWithDnsLookupFunction()
    : io_thread_(g_browser_process->io_thread()),
      request_handle_(new net::HostResolver::RequestHandle),
      addresses_(new net::AddressList) {
}

SocketExtensionWithDnsLookupFunction::~SocketExtensionWithDnsLookupFunction() {
}

void SocketExtensionWithDnsLookupFunction::StartDnsLookup(
    const std::string& hostname) {
  net::HostResolver* host_resolver =
      HostResolverWrapper::GetInstance()->GetHostResolver(
          io_thread_->globals()->host_resolver.get());
  DCHECK(host_resolver);

  // Yes, we are passing zero as the port. There are some interesting but not
  // presently relevant reasons why HostResolver asks for the port of the
  // hostname you'd like to resolve, even though it doesn't use that value in
  // determining its answer.
  net::HostPortPair host_port_pair(hostname, 0);

  net::HostResolver::RequestInfo request_info(host_port_pair);
  int resolve_result = host_resolver->Resolve(
      request_info, addresses_.get(),
      base::Bind(&SocketExtensionWithDnsLookupFunction::OnDnsLookup, this),
      request_handle_.get(), net::BoundNetLog());

  if (resolve_result != net::ERR_IO_PENDING)
    OnDnsLookup(resolve_result);
}

void SocketExtensionWithDnsLookupFunction::OnDnsLookup(int resolve_result) {
  if (resolve_result == net::OK) {
    DCHECK(!addresses_->empty());
    resolved_address_ = addresses_->front().ToStringWithoutPort();
  } else {
    error_ = kDnsLookupFailedError;
  }
  AfterDnsLookup(resolve_result);
}

SocketCreateFunction::SocketCreateFunction()
    : socket_type_(kSocketTypeInvalid) {
}

SocketCreateFunction::~SocketCreateFunction() {}

bool SocketCreateFunction::Prepare() {
  params_ = api::socket::Create::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  switch (params_->type) {
    case extensions::api::socket::SOCKET_TYPE_TCP:
      socket_type_ = kSocketTypeTCP;
      break;
    case extensions::api::socket::SOCKET_TYPE_UDP:
      socket_type_ = kSocketTypeUDP;
      break;
    case extensions::api::socket::SOCKET_TYPE_NONE:
      NOTREACHED();
      break;
  }

  return true;
}

void SocketCreateFunction::Work() {
  Socket* socket = NULL;
  if (socket_type_ == kSocketTypeTCP) {
    socket = new TCPSocket(extension_->id());
  } else if (socket_type_== kSocketTypeUDP) {
    socket = new UDPSocket(extension_->id());
  }
  DCHECK(socket);

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kSocketIdKey, manager_->Add(socket));
  SetResult(result);
}

bool SocketDestroyFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  return true;
}

void SocketDestroyFunction::Work() {
  RemoveSocket(socket_id_);
}

SocketConnectFunction::SocketConnectFunction()
    : socket_id_(0),
      port_(0) {
}

SocketConnectFunction::~SocketConnectFunction() {
}

bool SocketConnectFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &hostname_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(2, &port_));
  return true;
}

void SocketConnectFunction::AsyncWorkStart() {
  socket_ = GetSocket(socket_id_);
  if (!socket_) {
    error_ = kSocketNotFoundError;
    SetResult(Value::CreateIntegerValue(-1));
    AsyncWorkCompleted();
    return;
  }

  SocketPermissionRequest::OperationType operation_type;
  switch (socket_->GetSocketType()) {
    case Socket::TYPE_TCP:
      operation_type = SocketPermissionRequest::TCP_CONNECT;
      break;
    case Socket::TYPE_UDP:
      operation_type = SocketPermissionRequest::UDP_SEND_TO;
      break;
    default:
      NOTREACHED() << "Unknown socket type.";
      operation_type = SocketPermissionRequest::NONE;
      break;
  }

  SocketPermission::CheckParam param(operation_type, hostname_, port_);
  if (!GetExtension()->CheckAPIPermissionWithParam(APIPermission::kSocket,
        &param)) {
    error_ = kPermissionError;
    SetResult(Value::CreateIntegerValue(-1));
    AsyncWorkCompleted();
    return;
  }

  StartDnsLookup(hostname_);
}

void SocketConnectFunction::AfterDnsLookup(int lookup_result) {
  if (lookup_result == net::OK) {
    StartConnect();
  } else {
    SetResult(Value::CreateIntegerValue(lookup_result));
    AsyncWorkCompleted();
  }
}

void SocketConnectFunction::StartConnect() {
  socket_->Connect(resolved_address_, port_,
                   base::Bind(&SocketConnectFunction::OnConnect, this));
}

void SocketConnectFunction::OnConnect(int result) {
  SetResult(Value::CreateIntegerValue(result));
  AsyncWorkCompleted();
}

bool SocketDisconnectFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  return true;
}

void SocketDisconnectFunction::Work() {
  Socket* socket = GetSocket(socket_id_);
  if (socket)
    socket->Disconnect();
  else
    error_ = kSocketNotFoundError;
  SetResult(Value::CreateNullValue());
}

bool SocketBindFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &address_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(2, &port_));
  return true;
}

void SocketBindFunction::Work() {
  int result = -1;
  Socket* socket = GetSocket(socket_id_);

  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(Value::CreateIntegerValue(result));
    return;
  }

  if (socket->GetSocketType() == Socket::TYPE_UDP) {
    SocketPermission::CheckParam param(
        SocketPermissionRequest::UDP_BIND, address_, port_);
    if (!GetExtension()->CheckAPIPermissionWithParam(APIPermission::kSocket,
          &param)) {
      error_ = kPermissionError;
      SetResult(Value::CreateIntegerValue(result));
      return;
    }
  } else if (socket->GetSocketType() == Socket::TYPE_TCP) {
    error_ = kTCPSocketBindError;
    SetResult(Value::CreateIntegerValue(result));
    return;
  }

  result = socket->Bind(address_, port_);
  SetResult(Value::CreateIntegerValue(result));
}

SocketListenFunction::SocketListenFunction()
    : params_(NULL) {
}

SocketListenFunction::~SocketListenFunction() {}

bool SocketListenFunction::Prepare() {
  params_ = api::socket::Listen::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketListenFunction::Work() {
  int result = -1;

  Socket* socket = GetSocket(params_->socket_id);
  if (socket) {
    SocketPermission::CheckParam param(
        SocketPermissionRequest::TCP_LISTEN, params_->address, params_->port);
    if (!GetExtension()->CheckAPIPermissionWithParam(APIPermission::kSocket,
          &param)) {
      error_ = kPermissionError;
      SetResult(Value::CreateIntegerValue(result));
      return;
    }

    result = socket->Listen(
        params_->address,
        params_->port,
        params_->backlog.get() ? *params_->backlog.get() : 5,
        &error_);
  } else {
    error_ = kSocketNotFoundError;
  }

  SetResult(Value::CreateIntegerValue(result));
}

SocketAcceptFunction::SocketAcceptFunction()
  : params_(NULL) {
}

SocketAcceptFunction::~SocketAcceptFunction() {}

bool SocketAcceptFunction::Prepare() {
  params_ = api::socket::Accept::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketAcceptFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(params_->socket_id);
  if (socket) {
    socket->Accept(base::Bind(&SocketAcceptFunction::OnAccept, this));
  } else {
    error_ = kSocketNotFoundError;
    OnAccept(-1, NULL);
  }
}

void SocketAcceptFunction::OnAccept(int result_code,
                                    net::TCPClientSocket *socket) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kResultCodeKey, result_code);
  if (socket) {
    Socket *client_socket = new TCPSocket(socket, extension_id(), true);
    result->SetInteger(kSocketIdKey, manager_->Add(client_socket));
  }
  SetResult(result);

  AsyncWorkCompleted();
}

SocketReadFunction::SocketReadFunction()
    : params_(NULL) {
}

SocketReadFunction::~SocketReadFunction() {}

bool SocketReadFunction::Prepare() {
  params_ = api::socket::Read::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketReadFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    OnCompleted(-1, NULL);
    return;
  }

  socket->Read(params_->buffer_size.get() ? *params_->buffer_size.get() : 4096,
               base::Bind(&SocketReadFunction::OnCompleted, this));
}

void SocketReadFunction::OnCompleted(int bytes_read,
                                     scoped_refptr<net::IOBuffer> io_buffer) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kResultCodeKey, bytes_read);
  if (bytes_read > 0) {
    result->Set(kDataKey,
                base::BinaryValue::CreateWithCopiedBuffer(io_buffer->data(),
                                                          bytes_read));
  } else {
    result->Set(kDataKey, new base::BinaryValue());
  }
  SetResult(result);

  AsyncWorkCompleted();
}

SocketWriteFunction::SocketWriteFunction()
    : socket_id_(0),
      io_buffer_(NULL),
      io_buffer_size_(0) {
}

SocketWriteFunction::~SocketWriteFunction() {}

bool SocketWriteFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  base::BinaryValue *data = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBinary(1, &data));

  io_buffer_size_ = data->GetSize();
  io_buffer_ = new net::WrappedIOBuffer(data->GetBuffer());
  return true;
}

void SocketWriteFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(socket_id_);

  if (!socket) {
    error_ = kSocketNotFoundError;
    OnCompleted(-1);
    return;
  }

  socket->Write(io_buffer_, io_buffer_size_,
                base::Bind(&SocketWriteFunction::OnCompleted, this));
}

void SocketWriteFunction::OnCompleted(int bytes_written) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kBytesWrittenKey, bytes_written);
  SetResult(result);

  AsyncWorkCompleted();
}

SocketRecvFromFunction::SocketRecvFromFunction()
    : params_(NULL) {
}

SocketRecvFromFunction::~SocketRecvFromFunction() {}

bool SocketRecvFromFunction::Prepare() {
  params_ = api::socket::RecvFrom::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketRecvFromFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    OnCompleted(-1, NULL, std::string(), 0);
    return;
  }

  socket->RecvFrom(params_->buffer_size.get() ? *params_->buffer_size : 4096,
                   base::Bind(&SocketRecvFromFunction::OnCompleted, this));
}

void SocketRecvFromFunction::OnCompleted(int bytes_read,
                                         scoped_refptr<net::IOBuffer> io_buffer,
                                         const std::string& address,
                                         int port) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kResultCodeKey, bytes_read);
  if (bytes_read > 0) {
    result->Set(kDataKey,
                base::BinaryValue::CreateWithCopiedBuffer(io_buffer->data(),
                                                          bytes_read));
  } else {
    result->Set(kDataKey, new base::BinaryValue());
  }
  result->SetString(kAddressKey, address);
  result->SetInteger(kPortKey, port);
  SetResult(result);

  AsyncWorkCompleted();
}

SocketSendToFunction::SocketSendToFunction()
    : socket_id_(0),
      io_buffer_(NULL),
      io_buffer_size_(0),
      port_(0) {
}

SocketSendToFunction::~SocketSendToFunction() {}

bool SocketSendToFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  base::BinaryValue *data = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBinary(1, &data));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &hostname_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(3, &port_));

  io_buffer_size_ = data->GetSize();
  io_buffer_ = new net::WrappedIOBuffer(data->GetBuffer());
  return true;
}

void SocketSendToFunction::AsyncWorkStart() {
  socket_ = GetSocket(socket_id_);
  if (!socket_) {
    error_ = kSocketNotFoundError;
    SetResult(Value::CreateIntegerValue(-1));
    AsyncWorkCompleted();
    return;
  }

  if (socket_->GetSocketType() == Socket::TYPE_UDP) {
    SocketPermission::CheckParam param(SocketPermissionRequest::UDP_SEND_TO,
        hostname_, port_);
    if (!GetExtension()->CheckAPIPermissionWithParam(APIPermission::kSocket,
          &param)) {
      error_ = kPermissionError;
      SetResult(Value::CreateIntegerValue(-1));
      AsyncWorkCompleted();
      return;
    }
  }

  StartDnsLookup(hostname_);
}

void SocketSendToFunction::AfterDnsLookup(int lookup_result) {
  if (lookup_result == net::OK) {
    StartSendTo();
  } else {
    SetResult(Value::CreateIntegerValue(lookup_result));
    AsyncWorkCompleted();
  }
}

void SocketSendToFunction::StartSendTo() {
  socket_->SendTo(io_buffer_, io_buffer_size_, resolved_address_, port_,
                  base::Bind(&SocketSendToFunction::OnCompleted, this));
}

void SocketSendToFunction::OnCompleted(int bytes_written) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kBytesWrittenKey, bytes_written);
  SetResult(result);

  AsyncWorkCompleted();
}

SocketSetKeepAliveFunction::SocketSetKeepAliveFunction()
    : params_(NULL) {
}

SocketSetKeepAliveFunction::~SocketSetKeepAliveFunction() {}

bool SocketSetKeepAliveFunction::Prepare() {
  params_ = api::socket::SetKeepAlive::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketSetKeepAliveFunction::Work() {
  bool result = false;
  Socket* socket = GetSocket(params_->socket_id);
  if (socket) {
    int delay = 0;
    if (params_->delay.get())
      delay = *params_->delay;
    result = socket->SetKeepAlive(params_->enable, delay);
  } else {
    error_ = kSocketNotFoundError;
  }
  SetResult(Value::CreateBooleanValue(result));
}

SocketSetNoDelayFunction::SocketSetNoDelayFunction()
    : params_(NULL) {
}

SocketSetNoDelayFunction::~SocketSetNoDelayFunction() {}

bool SocketSetNoDelayFunction::Prepare() {
  params_ = api::socket::SetNoDelay::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketSetNoDelayFunction::Work() {
  bool result = false;
  Socket* socket = GetSocket(params_->socket_id);
  if (socket)
    result = socket->SetNoDelay(params_->no_delay);
  else
    error_ = kSocketNotFoundError;
  SetResult(Value::CreateBooleanValue(result));
}

SocketGetInfoFunction::SocketGetInfoFunction()
    : params_(NULL) {}

SocketGetInfoFunction::~SocketGetInfoFunction() {}

bool SocketGetInfoFunction::Prepare() {
  params_ = api::socket::GetInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketGetInfoFunction::Work() {
  api::socket::SocketInfo info;
  Socket* socket = GetSocket(params_->socket_id);
  if (socket) {
    // This represents what we know about the socket, and does not call through
    // to the system.
    if (socket->GetSocketType() == Socket::TYPE_TCP)
      info.socket_type = extensions::api::socket::SOCKET_TYPE_TCP;
    else
      info.socket_type = extensions::api::socket::SOCKET_TYPE_UDP;
    info.connected = socket->IsConnected();

    // Grab the peer address as known by the OS. This and the call below will
    // always succeed while the socket is connected, even if the socket has
    // been remotely closed by the peer; only reading the socket will reveal
    // that it should be closed locally.
    net::IPEndPoint peerAddress;
    if (socket->GetPeerAddress(&peerAddress)) {
      info.peer_address.reset(
          new std::string(peerAddress.ToStringWithoutPort()));
      info.peer_port.reset(new int(peerAddress.port()));
    }

    // Grab the local address as known by the OS.
    net::IPEndPoint localAddress;
    if (socket->GetLocalAddress(&localAddress)) {
      info.local_address.reset(
          new std::string(localAddress.ToStringWithoutPort()));
      info.local_port.reset(new int(localAddress.port()));
    }
  } else {
    error_ = kSocketNotFoundError;
  }
  SetResult(info.ToValue().release());
}

bool SocketGetNetworkListFunction::RunImpl() {
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&SocketGetNetworkListFunction::GetNetworkListOnFileThread,
          this));
  return true;
}

void SocketGetNetworkListFunction::GetNetworkListOnFileThread() {
  net::NetworkInterfaceList interface_list;
  if (GetNetworkList(&interface_list)) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&SocketGetNetworkListFunction::SendResponseOnUIThread,
            this, interface_list));
    return;
  }

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&SocketGetNetworkListFunction::HandleGetNetworkListError,
          this));
}

void SocketGetNetworkListFunction::HandleGetNetworkListError() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  error_ = kNetworkListError;
  SendResponse(false);
}

void SocketGetNetworkListFunction::SendResponseOnUIThread(
    const net::NetworkInterfaceList& interface_list) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::vector<linked_ptr<api::socket::NetworkInterface> > create_arg;
  create_arg.reserve(interface_list.size());
  for (net::NetworkInterfaceList::const_iterator i = interface_list.begin();
       i != interface_list.end(); ++i) {
    linked_ptr<api::socket::NetworkInterface> info =
        make_linked_ptr(new api::socket::NetworkInterface);
    info->name = i->name;
    info->address = net::IPAddressToString(i->address);
    create_arg.push_back(info);
  }

  results_ = api::socket::GetNetworkList::Results::Create(create_arg);
  SendResponse(true);
}

}  // namespace extensions
