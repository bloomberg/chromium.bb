// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/worker_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/pepper/pepper_lookup_request.h"
#include "content/browser/renderer_host/pepper/pepper_tcp_server_socket.h"
#include "content/browser/renderer_host/pepper/pepper_tcp_socket.h"
#include "content/browser/renderer_host/pepper/pepper_udp_socket.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/pepper_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/font_list_async.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/sys_addrinfo.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"
#include "ppapi/shared_impl/private/ppb_host_resolver_shared.h"

using ppapi::NetAddressPrivateImpl;

namespace content {
namespace {

const size_t kMaxSocketsAllowed = 1024;
const uint32 kInvalidSocketID = 0;

void CreateNetAddressListFromAddressList(
    const net::AddressList& list,
    std::vector<PP_NetAddress_Private>* net_address_list) {
  PP_NetAddress_Private address;
  for (size_t i = 0; i < list.size(); ++i) {
    if (!NetAddressPrivateImpl::IPEndPointToNetAddress(list[i].address(),
                                                       list[i].port(),
                                                       &address)) {
      net_address_list->clear();
      return;
    }
    net_address_list->push_back(address);
  }
}

}  // namespace

PepperMessageFilter::PepperMessageFilter(ProcessType type,
                                         int process_id,
                                         BrowserContext* browser_context)
    : process_type_(type),
      permissions_(),
      process_id_(process_id),
      nacl_render_view_id_(0),
      resource_context_(browser_context->GetResourceContext()),
      host_resolver_(NULL),
      next_socket_id_(1) {
  DCHECK(type == RENDERER);
  DCHECK(browser_context);
  // Keep BrowserContext data in FILE-thread friendly storage.
  browser_path_ = browser_context->GetPath();
  incognito_ = browser_context->IsOffTheRecord();
  DCHECK(resource_context_);
}

PepperMessageFilter::PepperMessageFilter(
    ProcessType type,
    const ppapi::PpapiPermissions& permissions,
    net::HostResolver* host_resolver)
    : process_type_(type),
      permissions_(permissions),
      process_id_(0),
      nacl_render_view_id_(0),
      resource_context_(NULL),
      host_resolver_(host_resolver),
      next_socket_id_(1),
      incognito_(false) {
  DCHECK(type == PLUGIN);
  DCHECK(host_resolver);
}

PepperMessageFilter::PepperMessageFilter(
    ProcessType type,
    const ppapi::PpapiPermissions& permissions,
    net::HostResolver* host_resolver,
    int process_id,
    int render_view_id)
    : process_type_(type),
      permissions_(permissions),
      process_id_(process_id),
      nacl_render_view_id_(render_view_id),
      resource_context_(NULL),
      host_resolver_(host_resolver),
      next_socket_id_(1) {
  DCHECK(type == NACL);
  DCHECK(host_resolver);
}

void PepperMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == PpapiHostMsg_PPBTCPServerSocket_Listen::ID ||
      message.type() == PpapiHostMsg_PPBTCPSocket_Connect::ID ||
      message.type() == PpapiHostMsg_PPBTCPSocket_ConnectWithNetAddress::ID ||
      message.type() == PpapiHostMsg_PPBUDPSocket_Bind::ID ||
      message.type() == PpapiHostMsg_PPBUDPSocket_SendTo::ID ||
      message.type() == PpapiHostMsg_PPBHostResolver_Resolve::ID) {
    *thread = BrowserThread::UI;
  }
}

bool PepperMessageFilter::OnMessageReceived(const IPC::Message& msg,
                                            bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PepperMessageFilter, msg, *message_was_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PpapiHostMsg_PPBInstance_GetFontFamilies,
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
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBUDPSocket_SetBoolSocketFeature,
                        OnUDPSetBoolSocketFeature)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBUDPSocket_Bind, OnUDPBind)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBUDPSocket_RecvFrom, OnUDPRecvFrom)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBUDPSocket_SendTo, OnUDPSendTo)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBUDPSocket_Close, OnUDPClose)

    // TCP Server messages.
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTCPServerSocket_Listen,
                        OnTCPServerListen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTCPServerSocket_Accept,
                        OnTCPServerAccept)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTCPServerSocket_Destroy,
                        RemoveTCPServerSocket)

    // HostResolver messages.
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBHostResolver_Resolve,
                        OnHostResolverResolve)

    // NetworkMonitor messages.
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBNetworkMonitor_Start,
                        OnNetworkMonitorStart)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBNetworkMonitor_Stop,
                        OnNetworkMonitorStop)

    // X509 certificate messages.
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBX509Certificate_ParseDER,
                        OnX509CertificateParseDER);

  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void PepperMessageFilter::OnIPAddressChanged() {
  GetAndSendNetworkList();
}

net::HostResolver* PepperMessageFilter::GetHostResolver() {
  return resource_context_ ?
      resource_context_->GetHostResolver() : host_resolver_;
}

net::CertVerifier* PepperMessageFilter::GetCertVerifier() {
  if (!cert_verifier_.get())
    cert_verifier_.reset(net::CertVerifier::CreateDefault());

  return cert_verifier_.get();
}

uint32 PepperMessageFilter::AddAcceptedTCPSocket(
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    net::StreamSocket* socket) {
  scoped_ptr<net::StreamSocket>  s(socket);

  uint32 tcp_socket_id = GenerateSocketID();
  if (tcp_socket_id != kInvalidSocketID) {
    tcp_sockets_[tcp_socket_id] = linked_ptr<PepperTCPSocket>(
        new PepperTCPSocket(this,
                            routing_id,
                            plugin_dispatcher_id,
                            tcp_socket_id,
                            s.release()));
  }
  return tcp_socket_id;
}

void PepperMessageFilter::RemoveTCPServerSocket(uint32 socket_id) {
  TCPServerSocketMap::iterator iter = tcp_server_sockets_.find(socket_id);
  if (iter == tcp_server_sockets_.end()) {
    NOTREACHED();
    return;
  }

  // Destroy the TCPServerSocket instance will cancel any pending completion
  // callback. From this point on, there won't be any messages associated with
  // this socket sent to the plugin side.
  tcp_server_sockets_.erase(iter);
}

PepperMessageFilter::~PepperMessageFilter() {
  if (!network_monitor_ids_.empty())
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void PepperMessageFilter::OnGetFontFamilies(IPC::Message* reply_msg) {
  GetFontListAsync(
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

void PepperMessageFilter::OnTCPConnect(int32 routing_id,
                                       uint32 socket_id,
                                       const std::string& host,
                                       uint16_t port) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::SocketPermissionRequest params(
      content::SocketPermissionRequest::TCP_CONNECT, host, port);
  bool allowed = CanUseSocketAPIs(routing_id, params);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperMessageFilter::DoTCPConnect, this,
          allowed, routing_id, socket_id, host, port));
}

void PepperMessageFilter::DoTCPConnect(bool allowed,
                                       int32 routing_id,
                                       uint32 socket_id,
                                       const std::string& host,
                                       uint16_t port) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  TCPSocketMap::iterator iter = tcp_sockets_.find(socket_id);
  if (iter == tcp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  if (routing_id == iter->second->routing_id() && allowed)
    iter->second->Connect(host, port);
  else
    iter->second->SendConnectACKError();
}

void PepperMessageFilter::OnTCPConnectWithNetAddress(
    int32 routing_id,
    uint32 socket_id,
    const PP_NetAddress_Private& net_addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool allowed = CanUseSocketAPIs(routing_id, CreateSocketPermissionRequest(
      content::SocketPermissionRequest::TCP_CONNECT, net_addr));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperMessageFilter::DoTCPConnectWithNetAddress, this,
          allowed, routing_id, socket_id, net_addr));
}

void PepperMessageFilter::DoTCPConnectWithNetAddress(
    bool allowed,
    int32 routing_id,
    uint32 socket_id,
    const PP_NetAddress_Private& net_addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  TCPSocketMap::iterator iter = tcp_sockets_.find(socket_id);
  if (iter == tcp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  if (routing_id == iter->second->routing_id() && allowed)
    iter->second->ConnectWithNetAddress(net_addr);
  else
    iter->second->SendConnectACKError();
}

void PepperMessageFilter::OnTCPSSLHandshake(
    uint32 socket_id,
    const std::string& server_name,
    uint16_t server_port,
    const std::vector<std::vector<char> >& trusted_certs,
    const std::vector<std::vector<char> >& untrusted_certs) {
  TCPSocketMap::iterator iter = tcp_sockets_.find(socket_id);
  if (iter == tcp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->SSLHandshake(server_name, server_port, trusted_certs,
                             untrusted_certs);
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

  // Destroying the TCPSocket instance will cancel any pending completion
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

void PepperMessageFilter::OnUDPSetBoolSocketFeature(
    int32 routing_id,
    uint32 socket_id,
    int32_t name,
    bool value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UDPSocketMap::iterator iter = udp_sockets_.find(socket_id);
  if (iter == udp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  if (routing_id != iter->second->routing_id()) {
    NOTREACHED();
    return;
  }

  switch(static_cast<PP_UDPSocketFeature_Private>(name)) {
    case PP_UDPSOCKETFEATURE_ADDRESS_REUSE:
      iter->second->AllowAddressReuse(value);
      break;
    case PP_UDPSOCKETFEATURE_BROADCAST:
      iter->second->AllowBroadcast(value);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void PepperMessageFilter::OnUDPBind(int32 routing_id,
                                    uint32 socket_id,
                                    const PP_NetAddress_Private& addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool allowed = CanUseSocketAPIs(routing_id, CreateSocketPermissionRequest(
      content::SocketPermissionRequest::UDP_BIND, addr));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperMessageFilter::DoUDPBind, this,
          allowed, routing_id, socket_id, addr));
}

void PepperMessageFilter::DoUDPBind(bool allowed,
                                    int32 routing_id,
                                    uint32 socket_id,
                                    const PP_NetAddress_Private& addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UDPSocketMap::iterator iter = udp_sockets_.find(socket_id);
  if (iter == udp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  if (routing_id == iter->second->routing_id() && allowed)
    iter->second->Bind(addr);
  else
    iter->second->SendBindACKError();
}

void PepperMessageFilter::OnUDPRecvFrom(uint32 socket_id, int32_t num_bytes) {
  UDPSocketMap::iterator iter = udp_sockets_.find(socket_id);
  if (iter == udp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->RecvFrom(num_bytes);
}

void PepperMessageFilter::OnUDPSendTo(int32 routing_id,
                                      uint32 socket_id,
                                      const std::string& data,
                                      const PP_NetAddress_Private& addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool allowed = CanUseSocketAPIs(routing_id, CreateSocketPermissionRequest(
      content::SocketPermissionRequest::UDP_SEND_TO, addr));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperMessageFilter::DoUDPSendTo, this,
          allowed, routing_id, socket_id, data, addr));

}

void PepperMessageFilter::DoUDPSendTo(bool allowed,
                                      int32 routing_id,
                                      uint32 socket_id,
                                      const std::string& data,
                                      const PP_NetAddress_Private& addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UDPSocketMap::iterator iter = udp_sockets_.find(socket_id);
  if (iter == udp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  if (routing_id == iter->second->routing_id() && allowed)
    iter->second->SendTo(data, addr);
  else
    iter->second->SendSendToACKError();
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

void PepperMessageFilter::OnTCPServerListen(int32 routing_id,
                                            uint32 plugin_dispatcher_id,
                                            PP_Resource socket_resource,
                                            const PP_NetAddress_Private& addr,
                                            int32_t backlog) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool allowed = CanUseSocketAPIs(routing_id, CreateSocketPermissionRequest(
      content::SocketPermissionRequest::TCP_LISTEN, addr));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&PepperMessageFilter::DoTCPServerListen,
                                     this,
                                     allowed,
                                     routing_id,
                                     plugin_dispatcher_id,
                                     socket_resource,
                                     addr,
                                     backlog));
}

void PepperMessageFilter::DoTCPServerListen(bool allowed,
                                            int32 routing_id,
                                            uint32 plugin_dispatcher_id,
                                            PP_Resource socket_resource,
                                            const PP_NetAddress_Private& addr,
                                            int32_t backlog) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!allowed) {
    Send(new PpapiMsg_PPBTCPServerSocket_ListenACK(routing_id,
                                                   plugin_dispatcher_id,
                                                   socket_resource,
                                                   0,
                                                   PP_ERROR_FAILED));
    return;
  }
  uint32 socket_id = GenerateSocketID();
  if (socket_id == kInvalidSocketID) {
    Send(new PpapiMsg_PPBTCPServerSocket_ListenACK(routing_id,
                                                   plugin_dispatcher_id,
                                                   socket_resource,
                                                   0,
                                                   PP_ERROR_NOSPACE));
    return;
  }
  PepperTCPServerSocket* socket = new PepperTCPServerSocket(
      this, routing_id, plugin_dispatcher_id, socket_resource, socket_id);
  tcp_server_sockets_[socket_id] = linked_ptr<PepperTCPServerSocket>(socket);
  socket->Listen(addr, backlog);
}

void PepperMessageFilter::OnTCPServerAccept(int32 tcp_client_socket_routing_id,
                                            uint32 server_socket_id) {
  TCPServerSocketMap::iterator iter =
      tcp_server_sockets_.find(server_socket_id);
  if (iter == tcp_server_sockets_.end()) {
    NOTREACHED();
    return;
  }
  iter->second->Accept(tcp_client_socket_routing_id);
}

void PepperMessageFilter::OnHostResolverResolve(
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    uint32 host_resolver_id,
    const ppapi::HostPortPair& host_port,
    const PP_HostResolver_Private_Hint& hint) {
  // Allow all process types except NaCl, unless the plugin is whitelisted for
  // using socket APIs.
  // TODO(bbudge) use app permissions when they are ready.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::SocketPermissionRequest request(
      content::SocketPermissionRequest::NONE, "", 0);
  if (process_type_ == NACL && !CanUseSocketAPIs(routing_id, request))
    return;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperMessageFilter::DoHostResolverResolve,
                 this,
                 routing_id,
                 plugin_dispatcher_id,
                 host_resolver_id,
                 host_port,
                 hint));
}

void PepperMessageFilter::DoHostResolverResolve(
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    uint32 host_resolver_id,
    const ppapi::HostPortPair& host_port,
    const PP_HostResolver_Private_Hint& hint) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::HostResolver::RequestInfo request_info(
      net::HostPortPair(host_port.host, host_port.port));

  net::AddressFamily address_family;
  switch (hint.family) {
    case PP_NETADDRESSFAMILY_IPV4:
      address_family = net::ADDRESS_FAMILY_IPV4;
      break;
    case PP_NETADDRESSFAMILY_IPV6:
      address_family = net::ADDRESS_FAMILY_IPV6;
      break;
    default:
      address_family = net::ADDRESS_FAMILY_UNSPECIFIED;
  }
  request_info.set_address_family(address_family);

  net::HostResolverFlags host_resolver_flags = 0;
  if (hint.flags & PP_HOST_RESOLVER_FLAGS_CANONNAME)
    host_resolver_flags |= net::HOST_RESOLVER_CANONNAME;
  if (hint.flags & PP_HOST_RESOLVER_FLAGS_LOOPBACK_ONLY)
    host_resolver_flags |= net::HOST_RESOLVER_LOOPBACK_ONLY;
  request_info.set_host_resolver_flags(host_resolver_flags);

  scoped_ptr<OnHostResolverResolveBoundInfo> bound_info(
      new OnHostResolverResolveBoundInfo);
  bound_info->routing_id = routing_id;
  bound_info->plugin_dispatcher_id = plugin_dispatcher_id;
  bound_info->host_resolver_id = host_resolver_id;

  // The lookup request will delete itself on completion.
  PepperLookupRequest<OnHostResolverResolveBoundInfo>* lookup_request =
      new PepperLookupRequest<OnHostResolverResolveBoundInfo>(
          GetHostResolver(),
          request_info,
          bound_info.release(),
          base::Bind(&PepperMessageFilter::OnHostResolverResolveLookupFinished,
                     this));
  lookup_request->Start();
}

void PepperMessageFilter::OnHostResolverResolveLookupFinished(
    int result,
    const net::AddressList& addresses,
    const OnHostResolverResolveBoundInfo& bound_info) {
  if (result != net::OK) {
    SendHostResolverResolveACKError(bound_info.routing_id,
                                    bound_info.plugin_dispatcher_id,
                                    bound_info.host_resolver_id);
  } else {
    const std::string& canonical_name = addresses.canonical_name();
    std::vector<PP_NetAddress_Private> net_address_list;
    CreateNetAddressListFromAddressList(addresses, &net_address_list);
    if (net_address_list.size() == 0) {
      SendHostResolverResolveACKError(bound_info.routing_id,
                                      bound_info.plugin_dispatcher_id,
                                      bound_info.host_resolver_id);
    } else {
      Send(new PpapiMsg_PPBHostResolver_ResolveACK(
          bound_info.routing_id,
          bound_info.plugin_dispatcher_id,
          bound_info.host_resolver_id,
          true,
          canonical_name,
          net_address_list));
    }
  }
}

bool PepperMessageFilter::SendHostResolverResolveACKError(
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    uint32 host_resolver_id) {
  return Send(new PpapiMsg_PPBHostResolver_ResolveACK(
      routing_id,
      plugin_dispatcher_id,
      host_resolver_id,
      false,
      "",
      std::vector<PP_NetAddress_Private>()));
}

void PepperMessageFilter::OnNetworkMonitorStart(uint32 plugin_dispatcher_id) {
  // Support all in-process plugins, and ones with "private" permissions.
  if (process_type_ != RENDERER &&
      !permissions_.HasPermission(ppapi::PERMISSION_PRIVATE))
    return;

  if (network_monitor_ids_.empty())
    net::NetworkChangeNotifier::AddIPAddressObserver(this);

  network_monitor_ids_.insert(plugin_dispatcher_id);
  GetAndSendNetworkList();
}

void PepperMessageFilter::OnNetworkMonitorStop(uint32 plugin_dispatcher_id) {
  // Support all in-process plugins, and ones with "private" permissions.
  if (process_type_ != RENDERER &&
      !permissions_.HasPermission(ppapi::PERMISSION_PRIVATE))
    return;

  network_monitor_ids_.erase(plugin_dispatcher_id);
  if (network_monitor_ids_.empty())
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void PepperMessageFilter::OnX509CertificateParseDER(
    const std::vector<char>& der,
    bool* succeeded,
    ppapi::PPB_X509Certificate_Fields* result) {
  if (der.size() == 0)
    *succeeded = false;
  *succeeded = PepperTCPSocket::GetCertificateFields(&der[0], der.size(),
                                                     result);
}

void PepperMessageFilter::GetFontFamiliesComplete(
    IPC::Message* reply_msg,
    scoped_ptr<base::ListValue> result) {
  std::string output;
  // If this is a NaCl plugin without private permission, we return an empty
  // string in |output|. We must reply to the message.
  if (process_type_ != NACL ||
      permissions_.HasPermission(ppapi::PERMISSION_PRIVATE)) {
    for (size_t i = 0; i < result->GetSize(); i++) {
      base::ListValue* cur_font;
      if (!result->GetList(i, &cur_font))
        continue;

      // Each entry is actually a list of (font name, localized name).
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
  }

  PpapiHostMsg_PPBInstance_GetFontFamilies::WriteReplyParams(reply_msg, output);
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
  if (tcp_sockets_.size() + udp_sockets_.size() + tcp_server_sockets_.size() >=
      kMaxSocketsAllowed) {
    return kInvalidSocketID;
  }

  uint32 socket_id = kInvalidSocketID;
  do {
    // Although it is unlikely, make sure that we won't cause any trouble when
    // the counter overflows.
    socket_id = next_socket_id_++;
  } while (socket_id == kInvalidSocketID ||
           tcp_sockets_.find(socket_id) != tcp_sockets_.end() ||
           udp_sockets_.find(socket_id) != udp_sockets_.end() ||
           tcp_server_sockets_.find(socket_id) != tcp_server_sockets_.end());

  return socket_id;
}

bool PepperMessageFilter::CanUseSocketAPIs(int32 render_id,
    const content::SocketPermissionRequest& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (process_type_ == PLUGIN) {
    // Always allow socket APIs for out-process plugins (except NACL).
    return true;
  }
  // NACL plugins always get their own PepperMessageFilter, initialized with
  // a render view id. Use this instead of the one that came with the message,
  // which is actually an API ID.
  if (process_type_ == NACL)
    render_id = nacl_render_view_id_;

  RenderViewHostImpl* render_view_host =
      RenderViewHostImpl::FromID(process_id_, render_id);
  if (!render_view_host)
    return false;

  SiteInstance* site_instance = render_view_host->GetSiteInstance();
  if (!site_instance)
    return false;

  if (!GetContentClient()->browser()->AllowPepperSocketAPI(
          site_instance->GetBrowserContext(),
          site_instance->GetSiteURL(),
          params)) {
    LOG(ERROR) << "Host " << site_instance->GetSiteURL().host()
               << " cannot use socket API or destination is not allowed";
    return false;
  }

  return true;
}

content::SocketPermissionRequest
PepperMessageFilter::CreateSocketPermissionRequest(
    content::SocketPermissionRequest::OperationType type,
    const PP_NetAddress_Private& net_addr) {
  std::string host = NetAddressPrivateImpl::DescribeNetAddress(net_addr, false);
  int port = 0;
  std::vector<unsigned char> address;
  NetAddressPrivateImpl::NetAddressToIPEndPoint(net_addr, &address, &port);
  return content::SocketPermissionRequest(type, host, port);
}

void PepperMessageFilter::GetAndSendNetworkList() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(&PepperMessageFilter::DoGetNetworkList, this));
}

void PepperMessageFilter::DoGetNetworkList() {
  scoped_ptr<net::NetworkInterfaceList> list(new net::NetworkInterfaceList());
  net::GetNetworkList(list.get());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperMessageFilter::SendNetworkList,
                 this, base::Passed(&list)));
}

void PepperMessageFilter::SendNetworkList(
    scoped_ptr<net::NetworkInterfaceList> list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_ptr< ::ppapi::NetworkList> list_copy(
      new ::ppapi::NetworkList(list->size()));
  for (size_t i = 0; i < list->size(); ++i) {
    const net::NetworkInterface& network = list->at(i);
    ::ppapi::NetworkInfo& network_copy = list_copy->at(i);
    network_copy.name = network.name;

    network_copy.addresses.resize(1, NetAddressPrivateImpl::kInvalidNetAddress);
    bool result = NetAddressPrivateImpl::IPEndPointToNetAddress(
        network.address, 0, &(network_copy.addresses[0]));
    DCHECK(result);

    // TODO(sergeyu): Currently net::NetworkInterfaceList provides
    // only name and one IP address. Add all other fields and copy
    // them here.
    network_copy.type = PP_NETWORKLIST_UNKNOWN;
    network_copy.state = PP_NETWORKLIST_UP;
    network_copy.display_name = network.name;
    network_copy.mtu = 0;
  }
  for (NetworkMonitorIdSet::iterator it = network_monitor_ids_.begin();
       it != network_monitor_ids_.end(); ++it) {
    Send(new PpapiMsg_PPBNetworkMonitor_NetworkList(
        ppapi::API_ID_PPB_NETWORKMANAGER_PRIVATE, *it, *list_copy));
  }
}

}  // namespace content
