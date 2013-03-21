// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/pepper/pepper_socket_utils.h"
#include "content/browser/renderer_host/pepper/pepper_tcp_server_socket.h"
#include "content/browser/renderer_host/pepper/pepper_tcp_socket.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/pepper_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/content_client.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/sys_addrinfo.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"

using ppapi::NetAddressPrivateImpl;

namespace content {
namespace {

const size_t kMaxSocketsAllowed = 1024;
const uint32 kInvalidSocketID = 0;

}  // namespace

PepperMessageFilter::PepperMessageFilter(int process_id,
                                         BrowserContext* browser_context)
    : plugin_type_(PLUGIN_TYPE_IN_PROCESS),
      permissions_(),
      process_id_(process_id),
      external_plugin_render_view_id_(0),
      resource_context_(browser_context->GetResourceContext()),
      host_resolver_(NULL),
      next_socket_id_(1) {
  DCHECK(browser_context);
  // Keep BrowserContext data in FILE-thread friendly storage.
  browser_path_ = browser_context->GetPath();
  incognito_ = browser_context->IsOffTheRecord();
  DCHECK(resource_context_);
}

PepperMessageFilter::PepperMessageFilter(
    const ppapi::PpapiPermissions& permissions,
    net::HostResolver* host_resolver)
    : plugin_type_(PLUGIN_TYPE_OUT_OF_PROCESS),
      permissions_(permissions),
      process_id_(0),
      external_plugin_render_view_id_(0),
      resource_context_(NULL),
      host_resolver_(host_resolver),
      next_socket_id_(1),
      incognito_(false) {
  DCHECK(host_resolver);
}

PepperMessageFilter::PepperMessageFilter(
    const ppapi::PpapiPermissions& permissions,
    net::HostResolver* host_resolver,
    int process_id,
    int render_view_id)
    : plugin_type_(PLUGIN_TYPE_EXTERNAL_PLUGIN),
      permissions_(permissions),
      process_id_(process_id),
      external_plugin_render_view_id_(render_view_id),
      resource_context_(NULL),
      host_resolver_(host_resolver),
      next_socket_id_(1) {
  DCHECK(host_resolver);
}

void PepperMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == PpapiHostMsg_PPBTCPServerSocket_Listen::ID ||
      message.type() == PpapiHostMsg_PPBTCPSocket_Connect::ID ||
      message.type() == PpapiHostMsg_PPBTCPSocket_ConnectWithNetAddress::ID) {
    *thread = BrowserThread::UI;
  }
}

bool PepperMessageFilter::OnMessageReceived(const IPC::Message& msg,
                                            bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PepperMessageFilter, msg, *message_was_ok)
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
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTCPSocket_SetBoolOption,
                        OnTCPSetBoolOption)

    // TCP Server messages.
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTCPServerSocket_Listen,
                        OnTCPServerListen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTCPServerSocket_Accept,
                        OnTCPServerAccept)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTCPServerSocket_Destroy,
                        RemoveTCPServerSocket)

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
    // Due to current permission check process (IO -> UI -> IO) some
    // calls to TCPSocketPrivate interface can be intermixed (like
    // Connect and Close). So, NOTREACHED() is not needed there.
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
  bool allowed = CanUseSocketAPIs(
      routing_id,
      pepper_socket_utils::CreateSocketPermissionRequest(
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
    // Due to current permission check process (IO -> UI -> IO) some
    // calls to TCPSocketPrivate interface can be intermixed (like
    // ConnectWithNetAddress and Close). So, NOTREACHED() is not
    // needed there.
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

void PepperMessageFilter::OnTCPSetBoolOption(uint32 socket_id,
                                             uint32_t name,
                                             bool value) {
  TCPSocketMap::iterator iter = tcp_sockets_.find(socket_id);
  if (iter == tcp_sockets_.end()) {
    NOTREACHED();
    return;
  }

  iter->second->SetBoolOption(name, value);
}

void PepperMessageFilter::OnTCPServerListen(int32 routing_id,
                                            uint32 plugin_dispatcher_id,
                                            PP_Resource socket_resource,
                                            const PP_NetAddress_Private& addr,
                                            int32_t backlog) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool allowed = CanUseSocketAPIs(
      routing_id,
      pepper_socket_utils::CreateSocketPermissionRequest(
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

void PepperMessageFilter::OnNetworkMonitorStart(uint32 plugin_dispatcher_id) {
  // Support all in-process plugins, and ones with "private" permissions.
  if (plugin_type_ != PLUGIN_TYPE_IN_PROCESS &&
      !permissions_.HasPermission(ppapi::PERMISSION_PRIVATE)) {
    return;
  }

  if (network_monitor_ids_.empty())
    net::NetworkChangeNotifier::AddIPAddressObserver(this);

  network_monitor_ids_.insert(plugin_dispatcher_id);
  GetAndSendNetworkList();
}

void PepperMessageFilter::OnNetworkMonitorStop(uint32 plugin_dispatcher_id) {
  // Support all in-process plugins, and ones with "private" permissions.
  if (plugin_type_ != PLUGIN_TYPE_IN_PROCESS &&
      !permissions_.HasPermission(ppapi::PERMISSION_PRIVATE)) {
    return;
  }

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
  if (tcp_sockets_.size() + tcp_server_sockets_.size() >= kMaxSocketsAllowed)
    return kInvalidSocketID;

  uint32 socket_id = kInvalidSocketID;
  do {
    // Although it is unlikely, make sure that we won't cause any trouble when
    // the counter overflows.
    socket_id = next_socket_id_++;
  } while (socket_id == kInvalidSocketID ||
           tcp_sockets_.find(socket_id) != tcp_sockets_.end() ||
           tcp_server_sockets_.find(socket_id) != tcp_server_sockets_.end());

  return socket_id;
}

bool PepperMessageFilter::CanUseSocketAPIs(int32 render_id,
    const content::SocketPermissionRequest& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // External plugins always get their own PepperMessageFilter, initialized with
  // a render view id. Use this instead of the one that came with the message,
  // which is actually an API ID.
  bool external_plugin = false;
  if (plugin_type_ == PLUGIN_TYPE_EXTERNAL_PLUGIN) {
    external_plugin = true;
    render_id = external_plugin_render_view_id_;
  }

  RenderViewHostImpl* render_view_host =
      RenderViewHostImpl::FromID(process_id_, render_id);

  return pepper_socket_utils::CanUseSocketAPIs(external_plugin,
                                               params,
                                               render_view_host);
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
