// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process_util.h"
#include "base/threading/worker_pool.h"
#include "base/values.h"
#include "content/browser/font_list_async.h"
#include "content/browser/resource_context.h"
#include "content/browser/renderer_host/pepper_tcp_socket.h"
#include "content/browser/renderer_host/pepper_udp_socket.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/pepper_messages.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/address_list.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/single_request_host_resolver.h"
#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"
#include "webkit/plugins/ppapi/ppb_flash_net_connector_impl.h"

#if defined(ENABLE_FLAPPER_HACKS)
#include <sys/types.h>
#include <unistd.h>
#endif  // ENABLE_FLAPPER_HACKS

using content::BrowserThread;
using ppapi::NetAddressPrivateImpl;

namespace {

const size_t kMaxSocketsAllowed = 1024;
const uint32 kInvalidSocketID = 0;

}  // namespace

PepperMessageFilter::PepperMessageFilter(
    const content::ResourceContext* resource_context)
    : resource_context_(resource_context),
      host_resolver_(NULL),
      next_socket_id_(1) {
  DCHECK(resource_context_);
}

PepperMessageFilter::PepperMessageFilter(net::HostResolver* host_resolver)
    : resource_context_(NULL),
      host_resolver_(host_resolver),
      next_socket_id_(1) {
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
    // TCP messages.
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTCPSocket_Create, OnTCPCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTCPSocket_Connect, OnTCPConnect)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTCPSocket_ConnectWithNetAddress,
                        OnTCPConnectWithNetAddress)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTCPSocket_SSLHandshake,
                        OnTCPSSLHandshake)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTCPSocket_Read, OnTCPRead)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTCPSocket_Write, OnTCPWrite)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTCPSocket_Disconnect, OnTCPDisconnect)

    // UDP messages.
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBUDPSocket_Create, OnUDPCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBUDPSocket_Bind, OnUDPBind)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBUDPSocket_RecvFrom, OnUDPRecvFrom)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBUDPSocket_SendTo, OnUDPSendTo)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBUDPSocket_Close, OnUDPClose)

    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

net::HostResolver* PepperMessageFilter::GetHostResolver() {
  return resource_context_ ?
      resource_context_->host_resolver() : host_resolver_;
}

net::CertVerifier* PepperMessageFilter::GetCertVerifier() {
  if (!cert_verifier_.get())
    cert_verifier_.reset(new net::CertVerifier());

  return cert_verifier_.get();
}

#if defined(ENABLE_FLAPPER_HACKS)

namespace {

int ConnectTcpSocket(const PP_NetAddress_Private& addr,
                     PP_NetAddress_Private* local_addr_out,
                     PP_NetAddress_Private* remote_addr_out) {
  *local_addr_out = NetAddressPrivateImpl::kInvalidNetAddress;
  *remote_addr_out = NetAddressPrivateImpl::kInvalidNetAddress;

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
  if (!NetAddressPrivateImpl::ValidateNetAddress(addr) ||
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
  return Send(new PepperMsg_ConnectTcpACK(
      routing_id, request_id, IPC::InvalidPlatformFileForTransit(),
      NetAddressPrivateImpl::kInvalidNetAddress,
      NetAddressPrivateImpl::kInvalidNetAddress));
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
  PP_NetAddress_Private local_addr = NetAddressPrivateImpl::kInvalidNetAddress;
  PP_NetAddress_Private remote_addr = NetAddressPrivateImpl::kInvalidNetAddress;
  PP_NetAddress_Private addr = NetAddressPrivateImpl::kInvalidNetAddress;

  for (const struct addrinfo* ai = addresses.head(); ai; ai = ai->ai_next) {
    if (NetAddressPrivateImpl::SockaddrToNetAddress(ai->ai_addr, ai->ai_addrlen,
                                                    &addr)) {
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
  PP_NetAddress_Private local_addr = NetAddressPrivateImpl::kInvalidNetAddress;
  PP_NetAddress_Private remote_addr = NetAddressPrivateImpl::kInvalidNetAddress;

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

void PepperMessageFilter::OnTCPCreate(int32 routing_id,
                                      uint32 plugin_dispatcher_id,
                                      uint32* socket_id) {
  *socket_id = GenerateSocketID();
  if (*socket_id == kInvalidSocketID)
    return;

  tcp_sockets_[*socket_id] = linked_ptr<PepperTCPSocket>(
      new PepperTCPSocket(this, routing_id, plugin_dispatcher_id, *socket_id));
}

void PepperMessageFilter::OnTCPConnect(uint32 socket_id,
                                       const std::string& host,
                                       uint16_t port) {
  TCPSocketMap::iterator iter = tcp_sockets_.find(socket_id);
  if (iter == tcp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->Connect(host, port);
}

void PepperMessageFilter::OnTCPConnectWithNetAddress(
    uint32 socket_id,
    const PP_NetAddress_Private& net_addr) {
  TCPSocketMap::iterator iter = tcp_sockets_.find(socket_id);
  if (iter == tcp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->ConnectWithNetAddress(net_addr);
}

void PepperMessageFilter::OnTCPSSLHandshake(uint32 socket_id,
                                            const std::string& server_name,
                                            uint16_t server_port) {
  TCPSocketMap::iterator iter = tcp_sockets_.find(socket_id);
  if (iter == tcp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->SSLHandshake(server_name, server_port);
}

void PepperMessageFilter::OnTCPRead(uint32 socket_id, int32_t bytes_to_read) {
  TCPSocketMap::iterator iter = tcp_sockets_.find(socket_id);
  if (iter == tcp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->Read(bytes_to_read);
}

void PepperMessageFilter::OnTCPWrite(uint32 socket_id,
                                     const std::string& data) {
  TCPSocketMap::iterator iter = tcp_sockets_.find(socket_id);
  if (iter == tcp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->Write(data);
}

void PepperMessageFilter::OnTCPDisconnect(uint32 socket_id) {
  TCPSocketMap::iterator iter = tcp_sockets_.find(socket_id);
  if (iter == tcp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  // Destroy the TCPSocket instance will cancel any pending completion
  // callback. From this point on, there won't be any messages associated with
  // this socket sent to the plugin side.
  tcp_sockets_.erase(iter);
}

void PepperMessageFilter::OnUDPCreate(int32 routing_id,
                                      uint32 plugin_dispatcher_id,
                                      uint32* socket_id) {
  *socket_id = GenerateSocketID();
  if (*socket_id == kInvalidSocketID)
    return;

  udp_sockets_[*socket_id] = linked_ptr<PepperUDPSocket>(
      new PepperUDPSocket(this, routing_id, plugin_dispatcher_id, *socket_id));
}

void PepperMessageFilter::OnUDPBind(uint32 socket_id,
                                    const PP_NetAddress_Private& addr) {
  UDPSocketMap::iterator iter = udp_sockets_.find(socket_id);
  if (iter == udp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->Bind(addr);
}

void PepperMessageFilter::OnUDPRecvFrom(uint32 socket_id, int32_t num_bytes) {
  UDPSocketMap::iterator iter = udp_sockets_.find(socket_id);
  if (iter == udp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->RecvFrom(num_bytes);
}

void PepperMessageFilter::OnUDPSendTo(uint32 socket_id,
                                      const std::string& data,
                                      const PP_NetAddress_Private& addr) {
  UDPSocketMap::iterator iter = udp_sockets_.find(socket_id);
  if (iter == udp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->SendTo(data, addr);
}

void PepperMessageFilter::OnUDPClose(uint32 socket_id) {
  UDPSocketMap::iterator iter = udp_sockets_.find(socket_id);
  if (iter == udp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  // Destroy the UDPSocket instance will cancel any pending completion
  // callback. From this point on, there won't be any messages associated with
  // this socket sent to the plugin side.
  udp_sockets_.erase(iter);
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

uint32 PepperMessageFilter::GenerateSocketID() {
  // TODO(yzshen): Change to use Pepper resource ID as socket ID.
  //
  // Generate a socket ID. For each process which sends us socket requests, IDs
  // of living sockets must be unique, to each socket type.
  //
  // However, it is safe to generate IDs based on the internal state of a single
  // PepperSocketMessageHandler object, because for each plugin or renderer
  // process, there is at most one PepperMessageFilter (in other words, at most
  // one PepperSocketMessageHandler) talking to it.
  if (tcp_sockets_.size() + udp_sockets_.size() >= kMaxSocketsAllowed)
    return kInvalidSocketID;

  uint32 socket_id = kInvalidSocketID;
  do {
    // Although it is unlikely, make sure that we won't cause any trouble when
    // the counter overflows.
    socket_id = next_socket_id_++;
  } while (socket_id == kInvalidSocketID ||
           tcp_sockets_.find(socket_id) != tcp_sockets_.end() ||
           udp_sockets_.find(socket_id) != udp_sockets_.end());

  return socket_id;
}
