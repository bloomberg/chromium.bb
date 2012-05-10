// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper_message_filter.h"

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
#include "content/browser/renderer_host/pepper_lookup_request.h"
#include "content/browser/renderer_host/pepper_tcp_server_socket.h"
#include "content/browser/renderer_host/pepper_tcp_socket.h"
#include "content/browser/renderer_host/pepper_udp_socket.h"
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

#ifdef OS_WIN
#include <windows.h>
#endif

using content::BrowserThread;
using content::RenderViewHostImpl;
using ppapi::NetAddressPrivateImpl;

namespace {

const size_t kMaxSocketsAllowed = 1024;
const uint32 kInvalidSocketID = 0;

// The ID is a 256-bit hash digest hex-encoded.
const int kDRMIdentifierSize = (256 / 8) * 2;
// The path to the file containing the DRM ID.
// It is mirrored from
//   chrome/browser/chromeos/system/drm_settings.cc
const char kDRMIdentifierFile[] = "Pepper DRM ID.0";

}  // namespace

PepperMessageFilter::PepperMessageFilter(
    ProcessType type,
    int process_id,
    content::BrowserContext* browser_context)
    : process_type_(type),
      process_id_(process_id),
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

PepperMessageFilter::PepperMessageFilter(ProcessType type,
                                         net::HostResolver* host_resolver)
    : process_type_(type),
      process_id_(0),
      resource_context_(NULL),
      host_resolver_(host_resolver),
      next_socket_id_(1) {
  DCHECK(type == PLUGIN);
  DCHECK(host_resolver);
}

void PepperMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == PpapiHostMsg_PPBTCPSocket_Connect::ID ||
      message.type() == PpapiHostMsg_PPBTCPSocket_ConnectWithNetAddress::ID ||
      message.type() == PpapiHostMsg_PPBUDPSocket_Bind::ID ||
      message.type() == PpapiHostMsg_PPBTCPServerSocket_Listen::ID ||
      message.type() == PpapiHostMsg_PPBHostResolver_Resolve::ID) {
    *thread = BrowserThread::UI;
  } else if (message.type() == PepperMsg_GetDeviceID::ID) {
    *thread = BrowserThread::FILE;
  }
}

bool PepperMessageFilter::OnMessageReceived(const IPC::Message& msg,
                                            bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PepperMessageFilter, msg, *message_was_ok)
    IPC_MESSAGE_HANDLER(PepperMsg_GetLocalTimeZoneOffset,
                        OnGetLocalTimeZoneOffset)
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

    // Flash messages.
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_UpdateActivity, OnUpdateActivity)
    IPC_MESSAGE_HANDLER(PepperMsg_GetDeviceID, OnGetDeviceID)

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

void PepperMessageFilter::OnTCPConnect(int32 routing_id,
                                       uint32 socket_id,
                                       const std::string& host,
                                       uint16_t port) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperMessageFilter::DoTCPConnect, this,
          CanUseSocketAPIs(routing_id), routing_id, socket_id, host, port));
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
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperMessageFilter::DoTCPConnectWithNetAddress, this,
          CanUseSocketAPIs(routing_id), routing_id, socket_id, net_addr));
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

void PepperMessageFilter::OnUDPBind(int32 routing_id,
                                    uint32 socket_id,
                                    const PP_NetAddress_Private& addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperMessageFilter::DoUDPBind, this,
          CanUseSocketAPIs(routing_id), routing_id, socket_id, addr));
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

void PepperMessageFilter::OnTCPServerListen(int32 routing_id,
                                            uint32 plugin_dispatcher_id,
                                            PP_Resource socket_resource,
                                            const PP_NetAddress_Private& addr,
                                            int32_t backlog) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&PepperMessageFilter::DoTCPServerListen,
                                     this,
                                     CanUseSocketAPIs(routing_id),
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperMessageFilter::DoHostResolverResolve, this,
                 CanUseSocketAPIs(routing_id),
                 routing_id,
                 plugin_dispatcher_id,
                 host_resolver_id,
                 host_port,
                 hint));
}

void PepperMessageFilter::DoHostResolverResolve(
    bool allowed,
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    uint32 host_resolver_id,
    const ppapi::HostPortPair& host_port,
    const PP_HostResolver_Private_Hint& hint) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!allowed) {
    SendHostResolverResolveACKError(routing_id,
                                    plugin_dispatcher_id,
                                    host_resolver_id);
    return;
  }

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
    scoped_ptr<ppapi::NetAddressList> net_address_list(
        ppapi::CreateNetAddressListFromAddressList(addresses));
    if (!net_address_list.get()) {
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
          *net_address_list.get()));
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
      ppapi::NetAddressList()));
}

void PepperMessageFilter::OnNetworkMonitorStart(uint32 plugin_dispatcher_id) {
  if (network_monitor_ids_.empty())
    net::NetworkChangeNotifier::AddIPAddressObserver(this);

  network_monitor_ids_.insert(plugin_dispatcher_id);
  GetAndSendNetworkList();
}

void PepperMessageFilter::OnNetworkMonitorStop(uint32 plugin_dispatcher_id) {
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

void PepperMessageFilter::OnUpdateActivity() {
#if defined(OS_WIN)
  // Reading then writing back the same value to the screensaver timeout system
  // setting resets the countdown which prevents the screensaver from turning
  // on "for a while". As long as the plugin pings us with this message faster
  // than the screensaver timeout, it won't go on.
  int value = 0;
  if (SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0, &value, 0))
    SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, value, NULL, 0);
#else
  // TODO(brettw) implement this for other platforms.
#endif
}

void PepperMessageFilter::OnGetDeviceID(std::string* id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  id->clear();

  // Grab the contents of the DRM identifier file.
  FilePath drm_id_file = browser_path_;
  drm_id_file = drm_id_file.AppendASCII(kDRMIdentifierFile);

  // This method should not be called with high frequency and its
  // useful to be able to validate use with a VLOG.
  VLOG(1) << "DRM ID requested @ " << drm_id_file.value();

  if (browser_path_.empty()) {
    LOG(ERROR) << "GetDeviceID requested from outside the RENDERER context.";
    return;
  }

  // Return an empty value when off the record.
  if (incognito_)
    return;

  // TODO(wad,brettw) Add OffTheRecord() enforcement here.
  // Normally this is left for the plugin to do, but in the
  // future we should check here as an added safeguard.

  char id_buf[kDRMIdentifierSize];
  if (file_util::ReadFile(drm_id_file, id_buf, kDRMIdentifierSize) !=
      kDRMIdentifierSize) {
    VLOG(1) << "file not readable: " << drm_id_file.value();
    return;
  }
  id->assign(id_buf, kDRMIdentifierSize);
}

void PepperMessageFilter::GetFontFamiliesComplete(
    IPC::Message* reply_msg,
    scoped_ptr<base::ListValue> result) {
  std::string output;
  for (size_t i = 0; i < result->GetSize(); i++) {
    base::ListValue* cur_font;
    if (!result->GetList(i, &cur_font))
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

bool PepperMessageFilter::CanUseSocketAPIs(int32 render_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (process_type_ == PLUGIN) {
    // Always allow socket APIs for out-process plugins.
    return true;
  }

  RenderViewHostImpl* render_view_host =
      RenderViewHostImpl::FromID(process_id_, render_id);
  if (!render_view_host)
    return false;

  content::SiteInstance* site_instance = render_view_host->GetSiteInstance();
  if (!site_instance)
    return false;

  if (!content::GetContentClient()->browser()->AllowSocketAPI(
          site_instance->GetBrowserContext(),
          site_instance->GetSite())) {
    LOG(ERROR) << "Host " << site_instance->GetSite().host()
               << " cannot use socket API";
    return false;
  }

  return true;
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
        net::IPEndPoint(network.address, 0), &(network_copy.addresses[0]));
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
