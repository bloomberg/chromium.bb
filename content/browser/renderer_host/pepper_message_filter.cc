// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper_message_filter.h"

#include <string.h>

#include <limits>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "base/threading/worker_pool.h"
#include "base/values.h"
#include "content/browser/font_list_async.h"
#include "content/browser/resource_context.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/pepper_messages.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/address_list.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/single_request_host_resolver.h"
#include "net/base/ssl_config_service.h"
#include "net/base/sys_addrinfo.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/udp/udp_server_socket.h"
#include "net/url_request/url_request_context.h"
#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_tcp_socket_private_proxy.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"
#include "webkit/plugins/ppapi/ppb_flash_net_connector_impl.h"

#if defined(ENABLE_FLAPPER_HACKS)
#include <sys/types.h>
#include <unistd.h>
#endif  // ENABLE_FLAPPER_HACKS

using content::BrowserThread;

namespace {

// TODO(viettrungluu): Move (some of) these to net_address_private_impl.h.
bool SockaddrToNetAddress(const sockaddr* sa,
                          socklen_t sa_length,
                          PP_NetAddress_Private* net_addr) {
  if (!sa || sa_length <= 0 || !net_addr)
    return false;

  CHECK_LE(static_cast<uint32_t>(sa_length), sizeof(net_addr->data));
  net_addr->size = sa_length;
  memcpy(net_addr->data, sa, net_addr->size);
  return true;
}

bool IPEndPointToNetAddress(const net::IPEndPoint& ip,
                            PP_NetAddress_Private* net_addr) {
  sockaddr_storage storage = { 0 };
  size_t length = sizeof(storage);

  return ip.ToSockAddr(reinterpret_cast<sockaddr*>(&storage), &length) &&
         SockaddrToNetAddress(reinterpret_cast<const sockaddr*>(&storage),
                              length, net_addr);
}

// Converts the first address to a PP_NetAddress_Private.
bool AddressListToNetAddress(const net::AddressList& address_list,
                             PP_NetAddress_Private* net_addr) {
  const addrinfo* head = address_list.head();
  return head &&
         SockaddrToNetAddress(head->ai_addr, head->ai_addrlen, net_addr);
}

bool NetAddressToIPEndPoint(const PP_NetAddress_Private& net_addr,
                            net::IPEndPoint* ip_end_point) {
  if (!ip_end_point ||
      !ppapi::NetAddressPrivateImpl::ValidateNetAddress(net_addr))
    return false;

  if (!ip_end_point->FromSockAddr(
      reinterpret_cast<const sockaddr*>(net_addr.data), net_addr.size)) {
    return false;
  }

  return true;
}

bool NetAddressToAddressList(const PP_NetAddress_Private& net_addr,
                             net::AddressList* address_list) {
  if (!address_list)
    return false;

  net::IPEndPoint ip_end_point;
  if (!NetAddressToIPEndPoint(net_addr, &ip_end_point))
    return false;

  *address_list = net::AddressList::CreateFromIPAddress(ip_end_point.address(),
                                                        ip_end_point.port());
  return true;
}

}  // namespace

// This assert fails on OpenBSD for an unknown reason at the moment.
#if !defined(OS_OPENBSD)
// Make sure the storage in |PP_NetAddress_Private| is big enough. (Do it here
// since the data is opaque elsewhere.)
COMPILE_ASSERT(sizeof(reinterpret_cast<PP_NetAddress_Private*>(0)->data) >=
               sizeof(sockaddr_storage), PP_NetAddress_Private_data_too_small);
#endif

const PP_NetAddress_Private kInvalidNetAddress = { 0 };

class PepperMessageFilter::TCPSocket {
 public:
  TCPSocket(TCPSocketManager* manager,
            int32 routing_id,
            uint32 plugin_dispatcher_id,
            uint32 socket_id);
  ~TCPSocket();

  void Connect(const std::string& host, uint16_t port);
  void ConnectWithNetAddress(const PP_NetAddress_Private& net_addr);
  void SSLHandshake(const std::string& server_name, uint16_t server_port);
  void Read(int32 bytes_to_read);
  void Write(const std::string& data);

 private:
  enum ConnectionState {
    // Before a connection is successfully established (including a previous
    // connect request failed).
    BEFORE_CONNECT,
    // There is a connect request that is pending.
    CONNECT_IN_PROGRESS,
    // A connection has been successfully established.
    CONNECTED,
    // There is an SSL handshake request that is pending.
    SSL_HANDSHAKE_IN_PROGRESS,
    // An SSL connection has been successfully established.
    SSL_CONNECTED,
    // An SSL handshake has failed.
    SSL_HANDSHAKE_FAILED
  };

  void StartConnect(const net::AddressList& addresses);

  void SendConnectACKError();
  void SendReadACKError();
  void SendWriteACKError();
  void SendSSLHandshakeACK(bool succeeded);

  void OnResolveCompleted(int result);
  void OnConnectCompleted(int result);
  void OnSSLHandshakeCompleted(int result);
  void OnReadCompleted(int result);
  void OnWriteCompleted(int result);

  bool IsConnected() const;

  TCPSocketManager* manager_;
  int32 routing_id_;
  uint32 plugin_dispatcher_id_;
  uint32 socket_id_;

  ConnectionState connection_state_;
  bool end_of_file_reached_;

  net::OldCompletionCallbackImpl<TCPSocket> connect_callback_;
  net::OldCompletionCallbackImpl<TCPSocket> ssl_handshake_callback_;
  net::OldCompletionCallbackImpl<TCPSocket> read_callback_;
  net::OldCompletionCallbackImpl<TCPSocket> write_callback_;

  scoped_ptr<net::SingleRequestHostResolver> resolver_;
  net::AddressList address_list_;

  scoped_ptr<net::StreamSocket> socket_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  scoped_refptr<net::IOBuffer> write_buffer_;

  DISALLOW_COPY_AND_ASSIGN(TCPSocket);
};

// Base class for TCP and UDP socket managers.
template<class SocketType>
class PepperMessageFilter::SocketManager {
 public:
  explicit SocketManager(PepperMessageFilter* pepper_message_filter);

 protected:
  // |socket_id| will be set to 0 on failure, non-zero otherwise.
  bool GenerateSocketID(uint32* socket_id);

  uint32 next_socket_id_;
  PepperMessageFilter* pepper_message_filter_;

  // SocketMap can hold either TCPSocket or UDPSocket.
  typedef std::map<uint32, linked_ptr<SocketType> > SocketMap;
  SocketMap sockets_;
};

template<class SocketType>
PepperMessageFilter::SocketManager<SocketType>::SocketManager(
    PepperMessageFilter* pepper_message_filter)
    : next_socket_id_(1),
      pepper_message_filter_(pepper_message_filter) {
  DCHECK(pepper_message_filter);
}

template<class SocketType>
bool PepperMessageFilter::SocketManager<SocketType>::GenerateSocketID(
    uint32* socket_id) {
  // Generate a socket ID. For each process which sends us socket requests, IDs
  // of living sockets must be unique, to each socket type.  TCP and UDP have
  // unique managers, so the socket ID can be the same in this case.
  //
  // However, it is safe to generate IDs based on the internal state of a single
  // SocketManager object, because for each plugin or renderer process,
  // there is at most one PepperMessageFilter talking to it

  if (sockets_.size() >= std::numeric_limits<uint32>::max()) {
    // All valid IDs are being used.
    *socket_id = 0;
    return false;
  }
  do {
    // Although it is unlikely, make sure that we won't cause any trouble when
    // the counter overflows.
    *socket_id = next_socket_id_++;
  } while (*socket_id == 0 ||
           sockets_.find(*socket_id) != sockets_.end());

  return true;
}

// TCPSocketManager manages the mapping from socket IDs to TCPSocket
// instances.
class PepperMessageFilter::TCPSocketManager : public SocketManager<TCPSocket> {
 public:
  explicit TCPSocketManager(PepperMessageFilter* pepper_message_filter);

  void OnMsgCreate(int32 routing_id,
                   uint32 plugin_dispatcher_id,
                   uint32* socket_id);
  void OnMsgConnect(uint32 socket_id,
                    const std::string& host,
                    uint16_t port);
  void OnMsgConnectWithNetAddress(uint32 socket_id,
                                  const PP_NetAddress_Private& net_addr);
  void OnMsgSSLHandshake(uint32 socket_id,
                         const std::string& server_name,
                         uint16_t server_port);
  void OnMsgRead(uint32 socket_id, int32_t bytes_to_read);
  void OnMsgWrite(uint32 socket_id, const std::string& data);
  void OnMsgDisconnect(uint32 socket_id);

  // Used by TCPSocket.
  bool Send(IPC::Message* message) {
    return pepper_message_filter_->Send(message);
  }
  net::HostResolver* GetHostResolver() {
    return pepper_message_filter_->GetHostResolver();
  }
  const net::SSLConfig& ssl_config() { return ssl_config_; }
  // The caller doesn't take ownership of the returned object.
  net::CertVerifier* GetCertVerifier();

 private:
  // The default SSL configuration settings are used, as opposed to Chrome's SSL
  // settings.
  net::SSLConfig ssl_config_;
  // This is lazily created. Users should use GetCertVerifier to retrieve it.
  scoped_ptr<net::CertVerifier> cert_verifier_;

  DISALLOW_COPY_AND_ASSIGN(TCPSocketManager);
};

PepperMessageFilter::TCPSocket::TCPSocket(
    TCPSocketManager* manager,
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    uint32 socket_id)
    : manager_(manager),
      routing_id_(routing_id),
      plugin_dispatcher_id_(plugin_dispatcher_id),
      socket_id_(socket_id),
      connection_state_(BEFORE_CONNECT),
      end_of_file_reached_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          connect_callback_(this, &TCPSocket::OnConnectCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          ssl_handshake_callback_(this, &TCPSocket::OnSSLHandshakeCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &TCPSocket::OnReadCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(this, &TCPSocket::OnWriteCompleted)) {
  DCHECK(manager);
}

PepperMessageFilter::TCPSocket::~TCPSocket() {
  // Make sure no further callbacks from socket_.
  if (socket_.get())
    socket_->Disconnect();
}

void PepperMessageFilter::TCPSocket::Connect(const std::string& host,
                                             uint16_t port) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (connection_state_ != BEFORE_CONNECT) {
    SendConnectACKError();
    return;
  }

  connection_state_ = CONNECT_IN_PROGRESS;
  net::HostResolver::RequestInfo request_info(net::HostPortPair(host, port));
  resolver_.reset(new net::SingleRequestHostResolver(
      manager_->GetHostResolver()));
  int result = resolver_->Resolve(
      request_info, &address_list_,
      base::Bind(&TCPSocket::OnResolveCompleted, base::Unretained(this)),
      net::BoundNetLog());
  if (result != net::ERR_IO_PENDING)
    OnResolveCompleted(result);
}

void PepperMessageFilter::TCPSocket::ConnectWithNetAddress(
    const PP_NetAddress_Private& net_addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (connection_state_ != BEFORE_CONNECT ||
      !NetAddressToAddressList(net_addr, &address_list_)) {
    SendConnectACKError();
    return;
  }

  connection_state_ = CONNECT_IN_PROGRESS;
  StartConnect(address_list_);
}

void PepperMessageFilter::TCPSocket::SSLHandshake(
    const std::string& server_name,
    uint16_t server_port) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Allow to do SSL handshake only if currently the socket has been connected
  // and there isn't pending read or write.
  // IsConnected() includes the state that SSL handshake has been finished and
  // therefore isn't suitable here.
  if (connection_state_ != CONNECTED || read_buffer_.get() ||
      write_buffer_.get()) {
    SendSSLHandshakeACK(false);
    return;
  }

  connection_state_ = SSL_HANDSHAKE_IN_PROGRESS;
  net::ClientSocketHandle* handle = new net::ClientSocketHandle();
  handle->set_socket(socket_.release());
  net::ClientSocketFactory* factory =
      net::ClientSocketFactory::GetDefaultFactory();
  net::HostPortPair host_port_pair(server_name, server_port);
  net::SSLClientSocketContext ssl_context;
  ssl_context.cert_verifier = manager_->GetCertVerifier();
  socket_.reset(factory->CreateSSLClientSocket(
      handle, host_port_pair, manager_->ssl_config(), NULL, ssl_context));
  if (!socket_.get()) {
    LOG(WARNING) << "Failed to create an SSL client socket.";
    OnSSLHandshakeCompleted(net::ERR_UNEXPECTED);
    return;
  }

  int result = socket_->Connect(&ssl_handshake_callback_);
  if (result != net::ERR_IO_PENDING)
    OnSSLHandshakeCompleted(result);
}

void PepperMessageFilter::TCPSocket::Read(int32 bytes_to_read) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!IsConnected() || end_of_file_reached_ || read_buffer_.get() ||
      bytes_to_read <= 0) {
    SendReadACKError();
    return;
  }

  if (bytes_to_read > ppapi::proxy::kTCPSocketMaxReadSize) {
    NOTREACHED();
    bytes_to_read = ppapi::proxy::kTCPSocketMaxReadSize;
  }

  read_buffer_ = new net::IOBuffer(bytes_to_read);
  int result = socket_->Read(read_buffer_, bytes_to_read, &read_callback_);
  if (result != net::ERR_IO_PENDING)
    OnReadCompleted(result);
}

void PepperMessageFilter::TCPSocket::Write(const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!IsConnected() || write_buffer_.get() || data.empty()) {
    SendWriteACKError();
    return;
  }

  int data_size = data.size();
  if (data_size > ppapi::proxy::kTCPSocketMaxWriteSize) {
    NOTREACHED();
    data_size = ppapi::proxy::kTCPSocketMaxWriteSize;
  }

  write_buffer_ = new net::IOBuffer(data_size);
  memcpy(write_buffer_->data(), data.c_str(), data_size);
  int result = socket_->Write(write_buffer_, data.size(), &write_callback_);
  if (result != net::ERR_IO_PENDING)
    OnWriteCompleted(result);
}

void PepperMessageFilter::TCPSocket::StartConnect(
    const net::AddressList& addresses) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS);

  socket_.reset(
      new net::TCPClientSocket(addresses, NULL, net::NetLog::Source()));
  int result = socket_->Connect(&connect_callback_);
  if (result != net::ERR_IO_PENDING)
    OnConnectCompleted(result);
}

void PepperMessageFilter::TCPSocket::SendConnectACKError() {
  manager_->Send(new PpapiMsg_PPBTCPSocket_ConnectACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, false,
      kInvalidNetAddress, kInvalidNetAddress));
}

void PepperMessageFilter::TCPSocket::SendReadACKError() {
  manager_->Send(new PpapiMsg_PPBTCPSocket_ReadACK(
    routing_id_, plugin_dispatcher_id_, socket_id_, false, std::string()));
}

void PepperMessageFilter::TCPSocket::SendWriteACKError() {
  manager_->Send(new PpapiMsg_PPBTCPSocket_WriteACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, false, 0));
}

void PepperMessageFilter::TCPSocket::SendSSLHandshakeACK(bool succeeded) {
  manager_->Send(new PpapiMsg_PPBTCPSocket_SSLHandshakeACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, succeeded));
}

void PepperMessageFilter::TCPSocket::OnResolveCompleted(int result) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS);

  if (result != net::OK) {
    SendConnectACKError();
    connection_state_ = BEFORE_CONNECT;
    return;
  }

  StartConnect(address_list_);
}

void PepperMessageFilter::TCPSocket::OnConnectCompleted(int result) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS && socket_.get());

  if (result != net::OK) {
    SendConnectACKError();
    connection_state_ = BEFORE_CONNECT;
  } else {
    net::IPEndPoint ip_end_point;
    net::AddressList address_list;
    PP_NetAddress_Private local_addr = kInvalidNetAddress;
    PP_NetAddress_Private remote_addr = kInvalidNetAddress;

    if (socket_->GetLocalAddress(&ip_end_point) != net::OK ||
        !IPEndPointToNetAddress(ip_end_point, &local_addr) ||
        socket_->GetPeerAddress(&address_list) != net::OK ||
        !AddressListToNetAddress(address_list, &remote_addr)) {
      SendConnectACKError();
      connection_state_ = BEFORE_CONNECT;
    } else {
      manager_->Send(new PpapiMsg_PPBTCPSocket_ConnectACK(
          routing_id_, plugin_dispatcher_id_, socket_id_, true,
          local_addr, remote_addr));
      connection_state_ = CONNECTED;
    }
  }
}

void PepperMessageFilter::TCPSocket::OnSSLHandshakeCompleted(int result) {
  DCHECK(connection_state_ == SSL_HANDSHAKE_IN_PROGRESS);

  bool succeeded = result == net::OK;
  SendSSLHandshakeACK(succeeded);
  connection_state_ = succeeded ? SSL_CONNECTED : SSL_HANDSHAKE_FAILED;
}

void PepperMessageFilter::TCPSocket::OnReadCompleted(int result) {
  DCHECK(read_buffer_.get());

  if (result > 0) {
    manager_->Send(new PpapiMsg_PPBTCPSocket_ReadACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true,
        std::string(read_buffer_->data(), result)));
  } else if (result == 0) {
    end_of_file_reached_ = true;
    manager_->Send(new PpapiMsg_PPBTCPSocket_ReadACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true, std::string()));
  } else {
    SendReadACKError();
  }
  read_buffer_ = NULL;
}

void PepperMessageFilter::TCPSocket::OnWriteCompleted(int result) {
  DCHECK(write_buffer_.get());

  if (result >= 0) {
    manager_->Send(new PpapiMsg_PPBTCPSocket_WriteACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true, result));
  } else {
    SendWriteACKError();
  }
  write_buffer_ = NULL;
}

bool PepperMessageFilter::TCPSocket::IsConnected() const {
  return connection_state_ == CONNECTED ||
         connection_state_ == SSL_CONNECTED;
}

class PepperMessageFilter::UDPSocket {
 public:
  UDPSocket(PepperMessageFilter* pepper_message_filter,
            int32 routing_id,
            uint32 plugin_dispatcher_id,
            uint32 socket_id);
  ~UDPSocket();

  void Bind(const PP_NetAddress_Private& addr);
  void RecvFrom(int32_t num_bytes);
  void SendTo(const std::string& data, const PP_NetAddress_Private& addr);

 private:
  void SendBindACK(bool result);
  void SendRecvFromACKError();
  void SendSendToACKError();

  void OnRecvFromCompleted(int result);
  void OnSendToCompleted(int result);

  PepperMessageFilter* pepper_message_filter_;
  int32 routing_id_;
  uint32 plugin_dispatcher_id_;
  uint32 socket_id_;

  net::OldCompletionCallbackImpl<UDPSocket> recvfrom_callback_;
  net::OldCompletionCallbackImpl<UDPSocket> sendto_callback_;

  scoped_ptr<net::UDPServerSocket> socket_;

  scoped_refptr<net::IOBuffer> recvfrom_buffer_;
  scoped_refptr<net::IOBuffer> sendto_buffer_;

  net::IPEndPoint recvfrom_address_;

  DISALLOW_COPY_AND_ASSIGN(UDPSocket);
};

PepperMessageFilter::UDPSocket::UDPSocket(
    PepperMessageFilter* pepper_message_filter,
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    uint32 socket_id)
    : pepper_message_filter_(pepper_message_filter),
      routing_id_(routing_id),
      plugin_dispatcher_id_(plugin_dispatcher_id),
      socket_id_(socket_id),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          recvfrom_callback_(this, &UDPSocket::OnRecvFromCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          sendto_callback_(this, &UDPSocket::OnSendToCompleted)) {
  DCHECK(pepper_message_filter);
}

PepperMessageFilter::UDPSocket::~UDPSocket() {
  // Make sure there are no further callbacks from socket_.
  if (socket_.get())
    socket_->Close();
}

void PepperMessageFilter::UDPSocket::Bind(
    const PP_NetAddress_Private& addr) {
  socket_.reset(new net::UDPServerSocket(NULL, net::NetLog::Source()));

  net::IPEndPoint address;
  if (!socket_.get() || !NetAddressToIPEndPoint(addr, &address)) {
    SendBindACK(false);
    return;
  }

  int result = socket_->Listen(address);

  SendBindACK(result == net::OK);
}

void PepperMessageFilter::UDPSocket::RecvFrom(int32_t num_bytes) {
  if (recvfrom_buffer_.get()) {
    SendRecvFromACKError();
    return;
  }

  recvfrom_buffer_ = new net::IOBuffer(num_bytes);
  int result = socket_->RecvFrom(recvfrom_buffer_,
                                 num_bytes,
                                 &recvfrom_address_,
                                 &recvfrom_callback_);

  if (result != net::ERR_IO_PENDING)
    OnRecvFromCompleted(result);
}

void PepperMessageFilter::UDPSocket::SendTo(
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  if (sendto_buffer_.get() || data.empty()) {
    SendSendToACKError();
    return;
  }

  net::IPEndPoint address;
  if (!NetAddressToIPEndPoint(addr, &address)) {
    SendSendToACKError();
    return;
  }

  int data_size = data.size();

  sendto_buffer_ = new net::IOBuffer(data_size);
  memcpy(sendto_buffer_->data(), data.data(), data_size);
  int result = socket_->SendTo(sendto_buffer_,
                               data_size,
                               address,
                               &sendto_callback_);

  if (result != net::ERR_IO_PENDING)
    OnSendToCompleted(result);
}

void PepperMessageFilter::UDPSocket::SendRecvFromACKError() {
  PP_NetAddress_Private addr = kInvalidNetAddress;
  pepper_message_filter_->Send(
      new PpapiMsg_PPBUDPSocket_RecvFromACK(
          routing_id_, plugin_dispatcher_id_, socket_id_, false,
          std::string(), addr));
}

void PepperMessageFilter::UDPSocket::SendSendToACKError() {
  pepper_message_filter_->Send(
      new PpapiMsg_PPBUDPSocket_SendToACK(
          routing_id_, plugin_dispatcher_id_, socket_id_, false, 0));
}

void PepperMessageFilter::UDPSocket::SendBindACK(bool result) {
  pepper_message_filter_->Send(
      new PpapiMsg_PPBUDPSocket_BindACK(
          routing_id_, plugin_dispatcher_id_, socket_id_, result));
}

void PepperMessageFilter::UDPSocket::OnRecvFromCompleted(int result) {
  DCHECK(recvfrom_buffer_.get());

  // Convert IPEndPoint we get back from RecvFrom to a PP_NetAddress_Private,
  // to send back.
  PP_NetAddress_Private addr = kInvalidNetAddress;
  if (!IPEndPointToNetAddress(recvfrom_address_, &addr) || result < 0) {
    SendRecvFromACKError();
  } else {
    pepper_message_filter_->Send(
        new PpapiMsg_PPBUDPSocket_RecvFromACK(
            routing_id_, plugin_dispatcher_id_, socket_id_, true,
            std::string(recvfrom_buffer_->data(), result), addr));
  }

  recvfrom_buffer_ = NULL;
}

void PepperMessageFilter::UDPSocket::OnSendToCompleted(int result) {
  DCHECK(sendto_buffer_.get());

  pepper_message_filter_->Send(
      new PpapiMsg_PPBUDPSocket_SendToACK(
          routing_id_, plugin_dispatcher_id_, socket_id_, true, result));

  sendto_buffer_ = NULL;
}

PepperMessageFilter::TCPSocketManager::TCPSocketManager(
    PepperMessageFilter* pepper_message_filter)
    : SocketManager<TCPSocket>(pepper_message_filter) {
}

void PepperMessageFilter::TCPSocketManager::OnMsgCreate(
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    uint32* socket_id) {
  if (!GenerateSocketID(socket_id))
    return;

  sockets_[*socket_id] = linked_ptr<TCPSocket>(
      new TCPSocket(this, routing_id, plugin_dispatcher_id, *socket_id));
}

void PepperMessageFilter::TCPSocketManager::OnMsgConnect(
    uint32 socket_id,
    const std::string& host,
    uint16_t port) {
  SocketMap::iterator iter = sockets_.find(socket_id);
  if (iter == sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->Connect(host, port);
}

void PepperMessageFilter::TCPSocketManager::OnMsgConnectWithNetAddress(
    uint32 socket_id,
    const PP_NetAddress_Private& net_addr) {
  SocketMap::iterator iter = sockets_.find(socket_id);
  if (iter == sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->ConnectWithNetAddress(net_addr);
}

void PepperMessageFilter::TCPSocketManager::OnMsgSSLHandshake(
    uint32 socket_id,
    const std::string& server_name,
    uint16_t server_port) {
  SocketMap::iterator iter = sockets_.find(socket_id);
  if (iter == sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->SSLHandshake(server_name, server_port);
}

void PepperMessageFilter::TCPSocketManager::OnMsgRead(
    uint32 socket_id,
    int32_t bytes_to_read) {
  SocketMap::iterator iter = sockets_.find(socket_id);
  if (iter == sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->Read(bytes_to_read);
}

void PepperMessageFilter::TCPSocketManager::OnMsgWrite(
    uint32 socket_id,
    const std::string& data) {
  SocketMap::iterator iter = sockets_.find(socket_id);
  if (iter == sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->Write(data);
}

void PepperMessageFilter::TCPSocketManager::OnMsgDisconnect(
    uint32 socket_id) {
  SocketMap::iterator iter = sockets_.find(socket_id);
  if (iter == sockets_.end()) {
    NOTREACHED();
    return;
  }

  // Destroy the TCPSocket instance will cancel any pending completion
  // callback. From this point on, there won't be any messages associated with
  // this socket sent to the plugin side.
  sockets_.erase(iter);
}

net::CertVerifier*
PepperMessageFilter::TCPSocketManager::GetCertVerifier() {
  if (!cert_verifier_.get())
    cert_verifier_.reset(new net::CertVerifier());

  return cert_verifier_.get();
}

// UDPSocketManager manages the mapping from socket IDs to UDPSocket
// instances.
class PepperMessageFilter::UDPSocketManager
    : public SocketManager<UDPSocket> {
 public:
  explicit UDPSocketManager(PepperMessageFilter* pepper_message_filter);

  void OnMsgCreate(int32 routing_id,
                   uint32 plugin_dispatcher_id,
                   uint32* socket_id);
  void OnMsgBind(uint32 socket_id,
                 const PP_NetAddress_Private& addr);
  void OnMsgRecvFrom(uint32 socket_id,
                     int32_t num_bytes);
  void OnMsgSendTo(uint32 socket_id,
                   const std::string& data,
                   const PP_NetAddress_Private& addr);
  void OnMsgClose(uint32 socket_id);

 private:
  DISALLOW_COPY_AND_ASSIGN(UDPSocketManager);
};

PepperMessageFilter::UDPSocketManager::UDPSocketManager(
    PepperMessageFilter* pepper_message_filter)
    : SocketManager<UDPSocket>(pepper_message_filter) {
}

void PepperMessageFilter::UDPSocketManager::OnMsgCreate(
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    uint32* socket_id) {
  if (!GenerateSocketID(socket_id))
    return;

  sockets_[*socket_id] = linked_ptr<UDPSocket>(
      new UDPSocket(pepper_message_filter_, routing_id,
                         plugin_dispatcher_id, *socket_id));
}

void PepperMessageFilter::UDPSocketManager::OnMsgBind(
    uint32 socket_id,
    const PP_NetAddress_Private& addr) {
  SocketMap::iterator iter = sockets_.find(socket_id);
  if (iter == sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->Bind(addr);
}

void PepperMessageFilter::UDPSocketManager::OnMsgRecvFrom(
    uint32 socket_id,
    int32_t num_bytes) {
  SocketMap::iterator iter = sockets_.find(socket_id);
  if (iter == sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->RecvFrom(num_bytes);
}

void PepperMessageFilter::UDPSocketManager::OnMsgSendTo(
    uint32 socket_id,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  SocketMap::iterator iter = sockets_.find(socket_id);
  if (iter == sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->SendTo(data, addr);
}

void PepperMessageFilter::UDPSocketManager::OnMsgClose(
    uint32 socket_id) {
  SocketMap::iterator iter = sockets_.find(socket_id);
  if (iter == sockets_.end()) {
    NOTREACHED();
    return;
  }

  // Destroy the UDPSocket instance will cancel any pending completion
  // callback. From this point on, there won't be any messages associated with
  // this socket sent to the plugin side.
  sockets_.erase(iter);
}

PepperMessageFilter::PepperMessageFilter(
    const content::ResourceContext* resource_context)
    : resource_context_(resource_context),
      host_resolver_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          socket_manager_tcp_(new TCPSocketManager(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          socket_manager_udp_(new UDPSocketManager(this))) {
  DCHECK(resource_context_);
}

PepperMessageFilter::PepperMessageFilter(net::HostResolver* host_resolver)
    : resource_context_(NULL),
      host_resolver_(host_resolver),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          socket_manager_tcp_(new TCPSocketManager(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          socket_manager_udp_(new UDPSocketManager(this))) {
  DCHECK(host_resolver);
}

PepperMessageFilter::~PepperMessageFilter() {}

bool PepperMessageFilter::OnMessageReceived(const IPC::Message& msg,
                                            bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PepperMessageFilter, msg, *message_was_ok)
#if defined(ENABLE_FLAPPER_HACKS)
    IPC_MESSAGE_HANDLER(PepperMsg_ConnectTcp, OnConnectTcp)
    IPC_MESSAGE_HANDLER(PepperMsg_ConnectTcpAddress, OnConnectTcpAddress)
#endif  // ENABLE_FLAPPER_HACKS
    IPC_MESSAGE_HANDLER(PepperMsg_GetLocalTimeZoneOffset,
                        OnGetLocalTimeZoneOffset)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PpapiHostMsg_PPBFont_GetFontFamilies,
                                    OnGetFontFamilies)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBTCPSocket_Create,
        socket_manager_tcp_.get(), TCPSocketManager::OnMsgCreate)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBTCPSocket_Connect,
        socket_manager_tcp_.get(), TCPSocketManager::OnMsgConnect)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBTCPSocket_ConnectWithNetAddress,
        socket_manager_tcp_.get(),
        TCPSocketManager::OnMsgConnectWithNetAddress)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBTCPSocket_SSLHandshake,
        socket_manager_tcp_.get(),
        TCPSocketManager::OnMsgSSLHandshake)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBTCPSocket_Read,
        socket_manager_tcp_.get(), TCPSocketManager::OnMsgRead)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBTCPSocket_Write,
        socket_manager_tcp_.get(), TCPSocketManager::OnMsgWrite)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBTCPSocket_Disconnect,
        socket_manager_tcp_.get(), TCPSocketManager::OnMsgDisconnect)

    // UDP
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBUDPSocket_Create,
        socket_manager_udp_.get(), UDPSocketManager::OnMsgCreate)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBUDPSocket_Bind,
        socket_manager_udp_.get(), UDPSocketManager::OnMsgBind)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBUDPSocket_RecvFrom,
        socket_manager_udp_.get(), UDPSocketManager::OnMsgRecvFrom)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBUDPSocket_SendTo,
        socket_manager_udp_.get(), UDPSocketManager::OnMsgSendTo)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBUDPSocket_Close,
        socket_manager_udp_.get(), UDPSocketManager::OnMsgClose)

    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

#if defined(ENABLE_FLAPPER_HACKS)

namespace {

int ConnectTcpSocket(const PP_NetAddress_Private& addr,
                     PP_NetAddress_Private* local_addr_out,
                     PP_NetAddress_Private* remote_addr_out) {
  *local_addr_out = kInvalidNetAddress;
  *remote_addr_out = kInvalidNetAddress;

  const struct sockaddr* sa =
      reinterpret_cast<const struct sockaddr*>(addr.data);
  socklen_t sa_len = addr.size;
  int fd = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
  if (fd == -1)
    return -1;
  if (connect(fd, sa, sa_len) != 0) {
    close(fd);
    return -1;
  }

  // Get the local address.
  socklen_t local_length = sizeof(local_addr_out->data);
  if (getsockname(fd, reinterpret_cast<struct sockaddr*>(local_addr_out->data),
                  &local_length) == -1 ||
      local_length > sizeof(local_addr_out->data)) {
    close(fd);
    return -1;
  }

  // The remote address is just the address we connected to.
  *remote_addr_out = addr;

  return fd;
}

}  // namespace

class PepperMessageFilter::LookupRequest {
 public:
  LookupRequest(PepperMessageFilter* pepper_message_filter,
                net::HostResolver* resolver,
                int routing_id,
                int request_id,
                const net::HostResolver::RequestInfo& request_info)
      : pepper_message_filter_(pepper_message_filter),
        resolver_(resolver),
        routing_id_(routing_id),
        request_id_(request_id),
        request_info_(request_info) {
  }

  void Start() {
    int result = resolver_.Resolve(
        request_info_, &addresses_,
        base::Bind(&LookupRequest::OnLookupFinished, base::Unretained(this)),
        net::BoundNetLog());
    if (result != net::ERR_IO_PENDING)
      OnLookupFinished(result);
  }

 private:
  void OnLookupFinished(int /*result*/) {
    pepper_message_filter_->ConnectTcpLookupFinished(
        routing_id_, request_id_, addresses_);
    delete this;
  }

  PepperMessageFilter* pepper_message_filter_;
  net::SingleRequestHostResolver resolver_;

  int routing_id_;
  int request_id_;
  net::HostResolver::RequestInfo request_info_;

  net::AddressList addresses_;

  DISALLOW_COPY_AND_ASSIGN(LookupRequest);
};

void PepperMessageFilter::OnConnectTcp(int routing_id,
                                       int request_id,
                                       const std::string& host,
                                       uint16 port) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::HostResolver::RequestInfo request_info(net::HostPortPair(host, port));

  // The lookup request will delete itself on completion.
  LookupRequest* lookup_request =
      new LookupRequest(this, GetHostResolver(),
                        routing_id, request_id, request_info);
  lookup_request->Start();
}

void PepperMessageFilter::OnConnectTcpAddress(
    int routing_id,
    int request_id,
    const PP_NetAddress_Private& addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Validate the address and then continue (doing |connect()|) on a worker
  // thread.
  if (!ppapi::NetAddressPrivateImpl::ValidateNetAddress(addr) ||
      !base::WorkerPool::PostTask(
          FROM_HERE,
          base::Bind(
              &PepperMessageFilter::ConnectTcpAddressOnWorkerThread, this,
              routing_id, request_id, addr),
          true)) {
    SendConnectTcpACKError(routing_id, request_id);
  }
}

bool PepperMessageFilter::SendConnectTcpACKError(int routing_id,
                                                 int request_id) {
  return Send(
      new PepperMsg_ConnectTcpACK(routing_id, request_id,
                                  IPC::InvalidPlatformFileForTransit(),
                                  kInvalidNetAddress, kInvalidNetAddress));
}

void PepperMessageFilter::ConnectTcpLookupFinished(
    int routing_id,
    int request_id,
    const net::AddressList& addresses) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // If the lookup returned addresses, continue (doing |connect()|) on a worker
  // thread.
  if (!addresses.head() ||
      !base::WorkerPool::PostTask(
          FROM_HERE,
          base::Bind(
              &PepperMessageFilter::ConnectTcpOnWorkerThread, this,
              routing_id, request_id, addresses),
          true)) {
    SendConnectTcpACKError(routing_id, request_id);
  }
}

void PepperMessageFilter::ConnectTcpOnWorkerThread(int routing_id,
                                                   int request_id,
                                                   net::AddressList addresses) {
  IPC::PlatformFileForTransit socket_for_transit =
      IPC::InvalidPlatformFileForTransit();
  PP_NetAddress_Private local_addr = kInvalidNetAddress;
  PP_NetAddress_Private remote_addr = kInvalidNetAddress;
  PP_NetAddress_Private addr = kInvalidNetAddress;

  for (const struct addrinfo* ai = addresses.head(); ai; ai = ai->ai_next) {
    if (SockaddrToNetAddress(ai->ai_addr, ai->ai_addrlen, &addr)) {
      int fd = ConnectTcpSocket(addr, &local_addr, &remote_addr);
      if (fd != -1) {
        socket_for_transit = base::FileDescriptor(fd, true);
        break;
      }
    }
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::IgnoreReturn<bool>(
          base::Bind(
              &PepperMessageFilter::Send, this,
              new PepperMsg_ConnectTcpACK(
                  routing_id, request_id,
                  socket_for_transit, local_addr, remote_addr))));
}

// TODO(vluu): Eliminate duplication between this and
// |ConnectTcpOnWorkerThread()|.
void PepperMessageFilter::ConnectTcpAddressOnWorkerThread(
    int routing_id,
    int request_id,
    PP_NetAddress_Private addr) {
  IPC::PlatformFileForTransit socket_for_transit =
      IPC::InvalidPlatformFileForTransit();
  PP_NetAddress_Private local_addr = kInvalidNetAddress;
  PP_NetAddress_Private remote_addr = kInvalidNetAddress;

  int fd = ConnectTcpSocket(addr, &local_addr, &remote_addr);
  if (fd != -1)
    socket_for_transit = base::FileDescriptor(fd, true);

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::IgnoreReturn<bool>(
          base::Bind(
              &PepperMessageFilter::Send, this,
              new PepperMsg_ConnectTcpACK(
                  routing_id, request_id,
                  socket_for_transit, local_addr, remote_addr))));
}

#endif  // ENABLE_FLAPPER_HACKS

void PepperMessageFilter::OnGetLocalTimeZoneOffset(base::Time t,
                                                   double* result) {
  // Explode it to local time and then unexplode it as if it were UTC. Also
  // explode it to UTC and unexplode it (this avoids mismatching rounding or
  // lack thereof). The time zone offset is their difference.
  //
  // The reason for this processing being in the browser process is that on
  // Linux, the localtime calls require filesystem access prohibited by the
  // sandbox.
  base::Time::Exploded exploded = { 0 };
  base::Time::Exploded utc_exploded = { 0 };
  t.LocalExplode(&exploded);
  t.UTCExplode(&utc_exploded);
  if (exploded.HasValidValues() && utc_exploded.HasValidValues()) {
    base::Time adj_time = base::Time::FromUTCExploded(exploded);
    base::Time cur = base::Time::FromUTCExploded(utc_exploded);
    *result = (adj_time - cur).InSecondsF();
  } else {
    *result = 0.0;
  }
}

void PepperMessageFilter::OnGetFontFamilies(IPC::Message* reply_msg) {
  content::GetFontListAsync(
      base::Bind(&PepperMessageFilter::GetFontFamiliesComplete,
                 this, reply_msg));
}

void PepperMessageFilter::GetFontFamiliesComplete(
    IPC::Message* reply_msg,
    scoped_refptr<content::FontListResult> result) {
  ListValue* input = result->list.get();

  std::string output;
  for (size_t i = 0; i < input->GetSize(); i++) {
    ListValue* cur_font;
    if (!input->GetList(i, &cur_font))
      continue;

    // Each entry in the list is actually a list of (font name, localized name).
    // We only care about the regular name.
    std::string font_name;
    if (!cur_font->GetString(0, &font_name))
      continue;

    // Font names are separated with nulls. We also want an explicit null at
    // the end of the string (Pepper strings aren't null terminated so since
    // we specify there will be a null, it should actually be in the string).
    output.append(font_name);
    output.push_back(0);
  }

  PpapiHostMsg_PPBFont_GetFontFamilies::WriteReplyParams(reply_msg, output);
  Send(reply_msg);
}

net::HostResolver* PepperMessageFilter::GetHostResolver() {
  if (resource_context_)
    return resource_context_->host_resolver();
  return host_resolver_;
}
