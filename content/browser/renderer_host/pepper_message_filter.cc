// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper_message_filter.h"

#include <string.h>

#include <limits>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "base/threading/worker_pool.h"
#include "base/values.h"
#include "content/browser/browser_thread.h"
#include "content/browser/font_list_async.h"
#include "content/browser/renderer_host/browser_render_process_host.h"
#include "content/browser/resource_context.h"
#include "content/common/pepper_messages.h"
#include "net/base/address_list.h"
#include "net/base/cert_verifier.h"
#include "net/base/ip_endpoint.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/single_request_host_resolver.h"
#include "net/base/ssl_config_service.h"
#include "net/base/sys_addrinfo.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/url_request/url_request_context.h"
#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_flash_tcp_socket_proxy.h"
#include "webkit/plugins/ppapi/ppb_flash_net_connector_impl.h"

#if defined(ENABLE_FLAPPER_HACKS)
#include <sys/types.h>
#include <unistd.h>
#endif  // ENABLE_FLAPPER_HACKS

namespace {

bool ValidateNetAddress(const PP_Flash_NetAddress& addr) {
  if (addr.size < sizeof(reinterpret_cast<sockaddr*>(0)->sa_family))
    return false;

  // TODO(viettrungluu): more careful validation?
  // Just do a size check for AF_INET.
  if (reinterpret_cast<const sockaddr*>(addr.data)->sa_family == AF_INET &&
      addr.size >= sizeof(sockaddr_in))
    return true;

  // Ditto for AF_INET6.
  if (reinterpret_cast<const sockaddr*>(addr.data)->sa_family == AF_INET6 &&
      addr.size >= sizeof(sockaddr_in6))
    return true;

  // Reject everything else.
  return false;
}

bool SockaddrToNetAddress(const sockaddr* sa,
                          socklen_t sa_length,
                          PP_Flash_NetAddress* net_addr) {
  if (!sa || sa_length <= 0 || !net_addr)
    return false;

  CHECK_LE(static_cast<uint32_t>(sa_length), sizeof(net_addr->data));
  net_addr->size = sa_length;
  memcpy(net_addr->data, sa, net_addr->size);
  return true;
}

bool IPEndPointToNetAddress(const net::IPEndPoint& ip,
                            PP_Flash_NetAddress* net_addr) {
  sockaddr_storage storage = { 0 };
  size_t length = sizeof(storage);

  return ip.ToSockAddr(reinterpret_cast<sockaddr*>(&storage), &length) &&
         SockaddrToNetAddress(reinterpret_cast<const sockaddr*>(&storage),
                              length, net_addr);
}

// Converts the first address to a PP_Flash_NetAddress.
bool AddressListToNetAddress(const net::AddressList& address_list,
                             PP_Flash_NetAddress* net_addr) {
  const addrinfo* head = address_list.head();
  return head &&
         SockaddrToNetAddress(head->ai_addr, head->ai_addrlen, net_addr);
}

bool NetAddressToAddressList(const PP_Flash_NetAddress& net_addr,
                             net::AddressList* address_list) {
  if (!address_list || !ValidateNetAddress(net_addr))
    return false;

  net::IPEndPoint ip_end_point;
  if (!ip_end_point.FromSockAddr(
      reinterpret_cast<const sockaddr*>(net_addr.data), net_addr.size)) {
    return false;
  }
  *address_list = net::AddressList::CreateFromIPAddress(ip_end_point.address(),
                                                        ip_end_point.port());
  return true;
}

}  // namespace

// Make sure the storage in |PP_Flash_NetAddress| is big enough. (Do it here
// since the data is opaque elsewhere.)
COMPILE_ASSERT(sizeof(reinterpret_cast<PP_Flash_NetAddress*>(0)->data) >=
               sizeof(sockaddr_storage), PP_Flash_NetAddress_data_too_small);

const PP_Flash_NetAddress kInvalidNetAddress = { 0 };

class PepperMessageFilter::FlashTCPSocket {
 public:
  FlashTCPSocket(FlashTCPSocketManager* manager,
                 int32 routing_id,
                 uint32 plugin_dispatcher_id,
                 uint32 socket_id);
  ~FlashTCPSocket();

  void Connect(const std::string& host, uint16_t port);
  void ConnectWithNetAddress(const PP_Flash_NetAddress& net_addr);
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

  FlashTCPSocketManager* manager_;
  int32 routing_id_;
  uint32 plugin_dispatcher_id_;
  uint32 socket_id_;

  ConnectionState connection_state_;
  bool end_of_file_reached_;

  net::CompletionCallbackImpl<FlashTCPSocket> resolve_callback_;
  net::CompletionCallbackImpl<FlashTCPSocket> connect_callback_;
  net::CompletionCallbackImpl<FlashTCPSocket> ssl_handshake_callback_;
  net::CompletionCallbackImpl<FlashTCPSocket> read_callback_;
  net::CompletionCallbackImpl<FlashTCPSocket> write_callback_;

  scoped_ptr<net::SingleRequestHostResolver> resolver_;
  net::AddressList address_list_;

  scoped_ptr<net::StreamSocket> socket_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  scoped_refptr<net::IOBuffer> write_buffer_;

  DISALLOW_COPY_AND_ASSIGN(FlashTCPSocket);
};

// FlashTCPSocketManager manages the mapping from socket IDs to FlashTCPSocket
// instances.
class PepperMessageFilter::FlashTCPSocketManager {
 public:
  explicit FlashTCPSocketManager(PepperMessageFilter* pepper_message_filter);

  void OnMsgCreate(int32 routing_id,
                   uint32 plugin_dispatcher_id,
                   uint32* socket_id);
  void OnMsgConnect(uint32 socket_id,
                    const std::string& host,
                    uint16_t port);
  void OnMsgConnectWithNetAddress(uint32 socket_id,
                                  const PP_Flash_NetAddress& net_addr);
  void OnMsgSSLHandshake(uint32 socket_id,
                         const std::string& server_name,
                         uint16_t server_port);
  void OnMsgRead(uint32 socket_id, int32_t bytes_to_read);
  void OnMsgWrite(uint32 socket_id, const std::string& data);
  void OnMsgDisconnect(uint32 socket_id);

  // Used by FlashTCPSocket.
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
  typedef std::map<uint32, linked_ptr<FlashTCPSocket> > SocketMap;
  SocketMap sockets_;

  uint32 next_socket_id_;

  PepperMessageFilter* pepper_message_filter_;

  // The default SSL configuration settings are used, as opposed to Chrome's SSL
  // settings.
  net::SSLConfig ssl_config_;
  // This is lazily created. Users should use GetCertVerifier to retrieve it.
  scoped_ptr<net::CertVerifier> cert_verifier_;

  DISALLOW_COPY_AND_ASSIGN(FlashTCPSocketManager);
};

PepperMessageFilter::FlashTCPSocket::FlashTCPSocket(
    FlashTCPSocketManager* manager,
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
          resolve_callback_(this, &FlashTCPSocket::OnResolveCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          connect_callback_(this, &FlashTCPSocket::OnConnectCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          ssl_handshake_callback_(this,
                                  &FlashTCPSocket::OnSSLHandshakeCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &FlashTCPSocket::OnReadCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(this, &FlashTCPSocket::OnWriteCompleted)) {
  DCHECK(manager);
}

PepperMessageFilter::FlashTCPSocket::~FlashTCPSocket() {
  // Make sure no further callbacks from socket_.
  if (socket_.get())
    socket_->Disconnect();
}

void PepperMessageFilter::FlashTCPSocket::Connect(const std::string& host,
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
  int result = resolver_->Resolve(request_info, &address_list_,
                                  &resolve_callback_, net::BoundNetLog());
  if (result != net::ERR_IO_PENDING)
    OnResolveCompleted(result);
}

void PepperMessageFilter::FlashTCPSocket::ConnectWithNetAddress(
    const PP_Flash_NetAddress& net_addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (connection_state_ != BEFORE_CONNECT ||
      !NetAddressToAddressList(net_addr, &address_list_)) {
    SendConnectACKError();
    return;
  }

  connection_state_ = CONNECT_IN_PROGRESS;
  StartConnect(address_list_);
}

void PepperMessageFilter::FlashTCPSocket::SSLHandshake(
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

void PepperMessageFilter::FlashTCPSocket::Read(int32 bytes_to_read) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!IsConnected() || end_of_file_reached_ || read_buffer_.get() ||
      bytes_to_read <= 0) {
    SendReadACKError();
    return;
  }

  if (bytes_to_read > pp::proxy::kFlashTCPSocketMaxReadSize) {
    NOTREACHED();
    bytes_to_read = pp::proxy::kFlashTCPSocketMaxReadSize;
  }

  read_buffer_ = new net::IOBuffer(bytes_to_read);
  int result = socket_->Read(read_buffer_, bytes_to_read, &read_callback_);
  if (result != net::ERR_IO_PENDING)
    OnReadCompleted(result);
}

void PepperMessageFilter::FlashTCPSocket::Write(const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!IsConnected() || write_buffer_.get() || data.empty()) {
    SendWriteACKError();
    return;
  }

  int data_size = data.size();
  if (data_size > pp::proxy::kFlashTCPSocketMaxWriteSize) {
    NOTREACHED();
    data_size = pp::proxy::kFlashTCPSocketMaxWriteSize;
  }

  write_buffer_ = new net::IOBuffer(data_size);
  memcpy(write_buffer_->data(), data.c_str(), data_size);
  int result = socket_->Write(write_buffer_, data.size(), &write_callback_);
  if (result != net::ERR_IO_PENDING)
    OnWriteCompleted(result);
}

void PepperMessageFilter::FlashTCPSocket::StartConnect(
    const net::AddressList& addresses) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS);

  socket_.reset(
      new net::TCPClientSocket(addresses, NULL, net::NetLog::Source()));
  int result = socket_->Connect(&connect_callback_);
  if (result != net::ERR_IO_PENDING)
    OnConnectCompleted(result);
}

void PepperMessageFilter::FlashTCPSocket::SendConnectACKError() {
  manager_->Send(new PpapiMsg_PPBFlashTCPSocket_ConnectACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, false,
      kInvalidNetAddress, kInvalidNetAddress));
}

void PepperMessageFilter::FlashTCPSocket::SendReadACKError() {
  manager_->Send(new PpapiMsg_PPBFlashTCPSocket_ReadACK(
    routing_id_, plugin_dispatcher_id_, socket_id_, false, std::string()));
}

void PepperMessageFilter::FlashTCPSocket::SendWriteACKError() {
  manager_->Send(new PpapiMsg_PPBFlashTCPSocket_WriteACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, false, 0));
}

void PepperMessageFilter::FlashTCPSocket::SendSSLHandshakeACK(bool succeeded) {
  manager_->Send(new PpapiMsg_PPBFlashTCPSocket_SSLHandshakeACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, succeeded));
}

void PepperMessageFilter::FlashTCPSocket::OnResolveCompleted(int result) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS);

  if (result != net::OK) {
    SendConnectACKError();
    connection_state_ = BEFORE_CONNECT;
    return;
  }

  StartConnect(address_list_);
}

void PepperMessageFilter::FlashTCPSocket::OnConnectCompleted(int result) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS && socket_.get());

  if (result != net::OK) {
    SendConnectACKError();
    connection_state_ = BEFORE_CONNECT;
  } else {
    net::IPEndPoint ip_end_point;
    net::AddressList address_list;
    PP_Flash_NetAddress local_addr = kInvalidNetAddress;
    PP_Flash_NetAddress remote_addr = kInvalidNetAddress;

    if (socket_->GetLocalAddress(&ip_end_point) != net::OK ||
        !IPEndPointToNetAddress(ip_end_point, &local_addr) ||
        socket_->GetPeerAddress(&address_list) != net::OK ||
        !AddressListToNetAddress(address_list, &remote_addr)) {
      SendConnectACKError();
      connection_state_ = BEFORE_CONNECT;
    } else {
      manager_->Send(new PpapiMsg_PPBFlashTCPSocket_ConnectACK(
          routing_id_, plugin_dispatcher_id_, socket_id_, true,
          local_addr, remote_addr));
      connection_state_ = CONNECTED;
    }
  }
}

void PepperMessageFilter::FlashTCPSocket::OnSSLHandshakeCompleted(int result) {
  DCHECK(connection_state_ == SSL_HANDSHAKE_IN_PROGRESS);

  bool succeeded = result == net::OK;
  SendSSLHandshakeACK(succeeded);
  connection_state_ = succeeded ? SSL_CONNECTED : SSL_HANDSHAKE_FAILED;
}

void PepperMessageFilter::FlashTCPSocket::OnReadCompleted(int result) {
  DCHECK(read_buffer_.get());

  if (result > 0) {
    manager_->Send(new PpapiMsg_PPBFlashTCPSocket_ReadACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true,
        std::string(read_buffer_->data(), result)));
  } else if (result == 0) {
    end_of_file_reached_ = true;
    manager_->Send(new PpapiMsg_PPBFlashTCPSocket_ReadACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true, std::string()));
  } else {
    SendReadACKError();
  }
  read_buffer_ = NULL;
}

void PepperMessageFilter::FlashTCPSocket::OnWriteCompleted(int result) {
  DCHECK(write_buffer_.get());

  if (result >= 0) {
    manager_->Send(new PpapiMsg_PPBFlashTCPSocket_WriteACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true, result));
  } else {
    SendWriteACKError();
  }
  write_buffer_ = NULL;
}

bool PepperMessageFilter::FlashTCPSocket::IsConnected() const {
  return connection_state_ == CONNECTED ||
         connection_state_ == SSL_CONNECTED;
}

PepperMessageFilter::FlashTCPSocketManager::FlashTCPSocketManager(
    PepperMessageFilter* pepper_message_filter)
    : next_socket_id_(1),
      pepper_message_filter_(pepper_message_filter) {
  DCHECK(pepper_message_filter);
}

void PepperMessageFilter::FlashTCPSocketManager::OnMsgCreate(
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    uint32* socket_id) {
  // Generate a socket ID. For each process which sends us socket requests, IDs
  // of living sockets must be unique.
  //
  // However, it is safe to generate IDs based on the internal state of a single
  // FlashTCPSocketManager object, because for each plugin or renderer process,
  // there is at most one PepperMessageFilter (in other words, at most one
  // FlashTCPSocketManager) talking to it.

  DCHECK(socket_id);
  if (sockets_.size() >= std::numeric_limits<uint32>::max()) {
    // All valid IDs are being used.
    *socket_id = 0;
    return;
  }
  do {
    // Although it is unlikely, make sure that we won't cause any trouble when
    // the counter overflows.
    *socket_id = next_socket_id_++;
  } while (*socket_id == 0 ||
           sockets_.find(*socket_id) != sockets_.end());
  sockets_[*socket_id] = linked_ptr<FlashTCPSocket>(
      new FlashTCPSocket(this, routing_id, plugin_dispatcher_id, *socket_id));
}

void PepperMessageFilter::FlashTCPSocketManager::OnMsgConnect(
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

void PepperMessageFilter::FlashTCPSocketManager::OnMsgConnectWithNetAddress(
    uint32 socket_id,
    const PP_Flash_NetAddress& net_addr) {
  SocketMap::iterator iter = sockets_.find(socket_id);
  if (iter == sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->ConnectWithNetAddress(net_addr);
}

void PepperMessageFilter::FlashTCPSocketManager::OnMsgSSLHandshake(
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

void PepperMessageFilter::FlashTCPSocketManager::OnMsgRead(
    uint32 socket_id,
    int32_t bytes_to_read) {
  SocketMap::iterator iter = sockets_.find(socket_id);
  if (iter == sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->Read(bytes_to_read);
}

void PepperMessageFilter::FlashTCPSocketManager::OnMsgWrite(
    uint32 socket_id,
    const std::string& data) {
  SocketMap::iterator iter = sockets_.find(socket_id);
  if (iter == sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->Write(data);
}

void PepperMessageFilter::FlashTCPSocketManager::OnMsgDisconnect(
    uint32 socket_id) {
  SocketMap::iterator iter = sockets_.find(socket_id);
  if (iter == sockets_.end()) {
    NOTREACHED();
    return;
  }

  // Destroy the FlashTCPSocket instance will cancel any pending completion
  // callback. From this point on, there won't be any messages associated with
  // this socket sent to the plugin side.
  sockets_.erase(iter);
}

net::CertVerifier*
PepperMessageFilter::FlashTCPSocketManager::GetCertVerifier() {
  if (!cert_verifier_.get())
    cert_verifier_.reset(new net::CertVerifier());

  return cert_verifier_.get();
}

PepperMessageFilter::PepperMessageFilter(
    const content::ResourceContext* resource_context)
    : resource_context_(resource_context),
      host_resolver_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          socket_manager_(new FlashTCPSocketManager(this))) {
  DCHECK(resource_context_);
}

PepperMessageFilter::PepperMessageFilter(net::HostResolver* host_resolver)
    : resource_context_(NULL),
      host_resolver_(host_resolver),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          socket_manager_(new FlashTCPSocketManager(this))) {
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
        PpapiHostMsg_PPBFlashTCPSocket_Create,
        socket_manager_.get(), FlashTCPSocketManager::OnMsgCreate)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBFlashTCPSocket_Connect,
        socket_manager_.get(), FlashTCPSocketManager::OnMsgConnect)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBFlashTCPSocket_ConnectWithNetAddress,
        socket_manager_.get(),
        FlashTCPSocketManager::OnMsgConnectWithNetAddress)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBFlashTCPSocket_SSLHandshake,
        socket_manager_.get(),
        FlashTCPSocketManager::OnMsgSSLHandshake)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBFlashTCPSocket_Read,
        socket_manager_.get(), FlashTCPSocketManager::OnMsgRead)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBFlashTCPSocket_Write,
        socket_manager_.get(), FlashTCPSocketManager::OnMsgWrite)
    IPC_MESSAGE_FORWARD(
        PpapiHostMsg_PPBFlashTCPSocket_Disconnect,
        socket_manager_.get(), FlashTCPSocketManager::OnMsgDisconnect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

#if defined(ENABLE_FLAPPER_HACKS)

namespace {

int ConnectTcpSocket(const PP_Flash_NetAddress& addr,
                     PP_Flash_NetAddress* local_addr_out,
                     PP_Flash_NetAddress* remote_addr_out) {
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
      : ALLOW_THIS_IN_INITIALIZER_LIST(
            net_callback_(this, &LookupRequest::OnLookupFinished)),
        pepper_message_filter_(pepper_message_filter),
        resolver_(resolver),
        routing_id_(routing_id),
        request_id_(request_id),
        request_info_(request_info) {
  }

  void Start() {
    int result = resolver_.Resolve(request_info_, &addresses_, &net_callback_,
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

  // HostResolver will call us using this callback when resolution is complete.
  net::CompletionCallbackImpl<LookupRequest> net_callback_;

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

void PepperMessageFilter::OnConnectTcpAddress(int routing_id,
                                              int request_id,
                                              const PP_Flash_NetAddress& addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Validate the address and then continue (doing |connect()|) on a worker
  // thread.
  if (!ValidateNetAddress(addr) ||
      !base::WorkerPool::PostTask(FROM_HERE,
           NewRunnableMethod(
               this,
               &PepperMessageFilter::ConnectTcpAddressOnWorkerThread,
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
      !base::WorkerPool::PostTask(FROM_HERE,
           NewRunnableMethod(
               this,
               &PepperMessageFilter::ConnectTcpOnWorkerThread,
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
  PP_Flash_NetAddress local_addr = kInvalidNetAddress;
  PP_Flash_NetAddress remote_addr = kInvalidNetAddress;
  PP_Flash_NetAddress addr = kInvalidNetAddress;

  for (const struct addrinfo* ai = addresses.head(); ai; ai = ai->ai_next) {
    if (SockaddrToNetAddress(ai->ai_addr, ai->ai_addrlen, &addr)) {
      int fd = ConnectTcpSocket(addr, &local_addr, &remote_addr);
      if (fd != -1) {
        socket_for_transit = base::FileDescriptor(fd, true);
        break;
      }
    }
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &PepperMessageFilter::Send,
          new PepperMsg_ConnectTcpACK(
              routing_id, request_id,
              socket_for_transit, local_addr, remote_addr)));
}

// TODO(vluu): Eliminate duplication between this and
// |ConnectTcpOnWorkerThread()|.
void PepperMessageFilter::ConnectTcpAddressOnWorkerThread(
    int routing_id,
    int request_id,
    PP_Flash_NetAddress addr) {
  IPC::PlatformFileForTransit socket_for_transit =
      IPC::InvalidPlatformFileForTransit();
  PP_Flash_NetAddress local_addr = kInvalidNetAddress;
  PP_Flash_NetAddress remote_addr = kInvalidNetAddress;

  int fd = ConnectTcpSocket(addr, &local_addr, &remote_addr);
  if (fd != -1)
    socket_for_transit = base::FileDescriptor(fd, true);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &PepperMessageFilter::Send,
          new PepperMsg_ConnectTcpACK(
              routing_id, request_id,
              socket_for_transit, local_addr, remote_addr)));
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
