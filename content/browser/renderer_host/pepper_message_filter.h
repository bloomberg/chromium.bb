// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_MESSAGE_FILTER_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "base/time.h"
#include "content/public/browser/browser_message_filter.h"
#include "net/base/network_change_notifier.h"
#include "net/base/net_util.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/stream_socket.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

class PepperTCPServerSocket;
class PepperTCPSocket;
class PepperUDPSocket;
struct PP_HostResolver_Private_Hint;
struct PP_NetAddress_Private;

namespace base {
class ListValue;
}

namespace content {
class BrowserContext;
class ResourceContext;
}

namespace net {
class AddressList;
class CertVerifier;
class HostResolver;
}

namespace ppapi {
struct HostPortPair;
class PPB_X509Certificate_Fields;
}

// This class is used in two contexts, both supporting PPAPI plugins. The first
// is on the renderer->browser channel, to handle requests from in-process
// PPAPI plugins and any requests that the PPAPI implementation code in the
// renderer needs to make. The second is on the plugin->browser channel to
// handle requests that out-of-process plugins send directly to the browser.
class PepperMessageFilter
    : public content::BrowserMessageFilter,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  enum ProcessType { PLUGIN, RENDERER };

  // Constructor when used in the context of a render process (the argument is
  // provided for sanity checking).
  PepperMessageFilter(ProcessType type,
                      int process_id,
                      content::BrowserContext* browser_context);

  // Constructor when used in the context of a PPAPI process (the argument is
  // provided for sanity checking).
  PepperMessageFilter(ProcessType type, net::HostResolver* host_resolver);

  // content::BrowserMessageFilter methods.
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // net::NetworkChangeNotifier::IPAddressObserver interface.
  virtual void OnIPAddressChanged() OVERRIDE;

  // Returns the host resolver (it may come from the resource context or the
  // host_resolver_ member).
  net::HostResolver* GetHostResolver();

  net::CertVerifier* GetCertVerifier();

  // Adds already accepted socket to the internal TCP sockets table. Takes
  // ownership over |socket|. In the case of failure (full socket table)
  // returns 0 and deletes |socket|. Otherwise, returns generated ID for
  // |socket|.
  uint32 AddAcceptedTCPSocket(int32 routing_id,
                              uint32 plugin_dispatcher_id,
                              net::StreamSocket* socket);
  void RemoveTCPServerSocket(uint32 socket_id);

  const net::SSLConfig& ssl_config() { return ssl_config_; }

 protected:
  virtual ~PepperMessageFilter();

 private:
  struct OnConnectTcpBoundInfo {
    int routing_id;
    int request_id;
  };

  struct OnHostResolverResolveBoundInfo {
    int32 routing_id;
    uint32 plugin_dispatcher_id;
    uint32 host_resolver_id;
  };

  // Containers for sockets keyed by socked_id.
  typedef std::map<uint32, linked_ptr<PepperTCPSocket> > TCPSocketMap;
  typedef std::map<uint32, linked_ptr<PepperUDPSocket> > UDPSocketMap;
  typedef std::map<uint32,
                   linked_ptr<PepperTCPServerSocket> > TCPServerSocketMap;

  // Set of disptachers ID's that have subscribed for NetworkMonitor
  // notifications.
  typedef std::set<uint32> NetworkMonitorIdSet;

  void OnGetLocalTimeZoneOffset(base::Time t, double* result);
  void OnGetFontFamilies(IPC::Message* reply);

  void OnTCPCreate(int32 routing_id,
                   uint32 plugin_dispatcher_id,
                   uint32* socket_id);
  void OnTCPConnect(int32 routing_id,
                    uint32 socket_id,
                    const std::string& host,
                    uint16_t port);
  void OnTCPConnectWithNetAddress(int32 routing_id,
                                  uint32 socket_id,
                                  const PP_NetAddress_Private& net_addr);
  void OnTCPSSLHandshake(
      uint32 socket_id,
      const std::string& server_name,
      uint16_t server_port,
      const std::vector<std::vector<char> >& trusted_certs,
      const std::vector<std::vector<char> >& untrusted_certs);
  void OnTCPRead(uint32 socket_id, int32_t bytes_to_read);
  void OnTCPWrite(uint32 socket_id, const std::string& data);
  void OnTCPDisconnect(uint32 socket_id);

  void OnUDPCreate(int32 routing_id,
                   uint32 plugin_dispatcher_id,
                   uint32* socket_id);
  void OnUDPBind(int32 routing_id,
                 uint32 socket_id,
                 const PP_NetAddress_Private& addr);
  void OnUDPRecvFrom(uint32 socket_id, int32_t num_bytes);
  void OnUDPSendTo(uint32 socket_id,
                   const std::string& data,
                   const PP_NetAddress_Private& addr);
  void OnUDPClose(uint32 socket_id);

  void OnTCPServerListen(int32 routing_id,
                         uint32 plugin_dispatcher_id,
                         PP_Resource socket_resource,
                         const PP_NetAddress_Private& addr,
                         int32_t backlog);
  void OnTCPServerAccept(int32 tcp_client_socket_routing_id,
                         uint32 server_socket_id);

  void OnHostResolverResolve(int32 routing_id,
                             uint32 plugin_dispatcher_id,
                             uint32 host_resolver_id,
                             const ppapi::HostPortPair& host_port,
                             const PP_HostResolver_Private_Hint& hint);
  // Continuation of |OnHostResolverResolve()|.
  void OnHostResolverResolveLookupFinished(
      int result,
      const net::AddressList& addresses,
      const OnHostResolverResolveBoundInfo& bound_info);
  bool SendHostResolverResolveACKError(int32 routing_id,
                                       uint32 plugin_dispatcher_id,
                                       uint32 host_resolver_id);

  void OnNetworkMonitorStart(uint32 plugin_dispatcher_id);
  void OnNetworkMonitorStop(uint32 plugin_dispatcher_id);

  void DoTCPConnect(bool allowed,
                    int32 routing_id,
                    uint32 socket_id,
                    const std::string& host,
                    uint16_t port);
  void DoTCPConnectWithNetAddress(bool allowed,
                                  int32 routing_id,
                                  uint32 socket_id,
                                  const PP_NetAddress_Private& net_addr);
  void DoUDPBind(bool allowed,
                 int32 routing_id,
                 uint32 socket_id,
                 const PP_NetAddress_Private& addr);
  void DoTCPServerListen(bool allowed,
                         int32 routing_id,
                         uint32 plugin_dispatcher_id,
                         PP_Resource socket_resource,
                         const PP_NetAddress_Private& addr,
                         int32_t backlog);
  void DoHostResolverResolve(bool allowed,
                             int32 routing_id,
                             uint32 plugin_dispatcher_id,
                             uint32 host_resolver_id,
                             const ppapi::HostPortPair& host_port,
                             const PP_HostResolver_Private_Hint& hint);

  void OnX509CertificateParseDER(const std::vector<char>& der,
                                 bool* succeeded,
                                 ppapi::PPB_X509Certificate_Fields* result);
  void OnUpdateActivity();
  void OnGetDeviceID(std::string* id);

  // Callback when the font list has been retrieved on a background thread.
  void GetFontFamiliesComplete(IPC::Message* reply_msg,
                               scoped_ptr<base::ListValue> result);

  uint32 GenerateSocketID();

  // Return true if render with given ID can use socket APIs.
  bool CanUseSocketAPIs(int32 render_id);

  void GetAndSendNetworkList();
  void DoGetNetworkList();
  void SendNetworkList(scoped_ptr<net::NetworkInterfaceList> list);

  ProcessType process_type_;

  // Render process ID.
  int process_id_;

  // When non-NULL, this should be used instead of the host_resolver_.
  content::ResourceContext* const resource_context_;

  // When non-NULL, this should be used instead of the resource_context_. Use
  // GetHostResolver instead of accessing directly.
  net::HostResolver* host_resolver_;

  // The default SSL configuration settings are used, as opposed to Chrome's SSL
  // settings.
  net::SSLConfig ssl_config_;
  // This is lazily created. Users should use GetCertVerifier to retrieve it.
  scoped_ptr<net::CertVerifier> cert_verifier_;

  uint32 next_socket_id_;

  TCPSocketMap tcp_sockets_;
  UDPSocketMap udp_sockets_;
  TCPServerSocketMap tcp_server_sockets_;

  NetworkMonitorIdSet network_monitor_ids_;

  FilePath browser_path_;

  DISALLOW_COPY_AND_ASSIGN(PepperMessageFilter);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_MESSAGE_FILTER_H_
