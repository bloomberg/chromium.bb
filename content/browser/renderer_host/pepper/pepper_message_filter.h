// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_MESSAGE_FILTER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/process_type.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/stream_socket.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

struct PP_NetAddress_Private;

namespace base {
class ListValue;
}

namespace net {
class CertVerifier;
class HostResolver;
}

namespace ppapi {
class PPB_X509Certificate_Fields;
}

namespace content {
class BrowserContext;
class PepperTCPServerSocket;
class PepperTCPSocket;
class ResourceContext;

// This class is used in two contexts, both supporting PPAPI plugins. The first
// is on the renderer->browser channel, to handle requests from in-process
// PPAPI plugins and any requests that the PPAPI implementation code in the
// renderer needs to make. The second is on the plugin->browser channel to
// handle requests that out-of-process plugins send directly to the browser.
class PepperMessageFilter
    : public BrowserMessageFilter,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  // Constructor when used in the context of a render process (the argument is
  // provided for sanity checking and must be PROCESS_TYPE_RENDERER).
  PepperMessageFilter(ProcessType process_type,
                      int process_id,
                      BrowserContext* browser_context);

  // Constructor when used in the context of a PPAPI process (the argument is
  // provided for sanity checking and must be PROCESS_TYPE_PPAPI_PLUGIN).
  PepperMessageFilter(ProcessType process_type,
                      const ppapi::PpapiPermissions& permissions,
                      net::HostResolver* host_resolver);

  // Constructor when used in the context of a NaCl process (the argument is
  // provided for sanity checking and must be PROCESS_TYPE_NACL_LOADER).
  PepperMessageFilter(ProcessType process_type,
                      const ppapi::PpapiPermissions& permissions,
                      net::HostResolver* host_resolver,
                      int process_id,
                      int render_view_id);

  // BrowserMessageFilter methods.
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      BrowserThread::ID* thread) OVERRIDE;
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

  // Containers for sockets keyed by socked_id.
  typedef std::map<uint32, linked_ptr<PepperTCPSocket> > TCPSocketMap;
  typedef std::map<uint32,
                   linked_ptr<PepperTCPServerSocket> > TCPServerSocketMap;

  // Set of disptachers ID's that have subscribed for NetworkMonitor
  // notifications.
  typedef std::set<uint32> NetworkMonitorIdSet;

  void OnGetLocalTimeZoneOffset(base::Time t, double* result);

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
  void OnTCPSetBoolOption(uint32 socket_id, uint32_t name, bool value);
  void OnTCPServerListen(int32 routing_id,
                         uint32 plugin_dispatcher_id,
                         PP_Resource socket_resource,
                         const PP_NetAddress_Private& addr,
                         int32_t backlog);
  void OnTCPServerAccept(int32 tcp_client_socket_routing_id,
                         uint32 server_socket_id);

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
  void DoTCPServerListen(bool allowed,
                         int32 routing_id,
                         uint32 plugin_dispatcher_id,
                         PP_Resource socket_resource,
                         const PP_NetAddress_Private& addr,
                         int32_t backlog);
  void OnX509CertificateParseDER(const std::vector<char>& der,
                                 bool* succeeded,
                                 ppapi::PPB_X509Certificate_Fields* result);
  void OnUpdateActivity();

  uint32 GenerateSocketID();

  // Return true if render with given ID can use socket APIs.
  bool CanUseSocketAPIs(int32 render_id,
      const content::SocketPermissionRequest& params);

  void GetAndSendNetworkList();
  void DoGetNetworkList();
  void SendNetworkList(scoped_ptr<net::NetworkInterfaceList> list);

  ProcessType process_type_;

  // When attached to an out-of-process plugin (be it native or NaCl) this
  // will have the Pepper permissions for the plugin. When attached to the
  // renderer channel, this will have no permissions listed (since there may
  // be many plugins sharing this channel).
  ppapi::PpapiPermissions permissions_;

  // Render process ID.
  int process_id_;

  // NACL RenderView id to determine private API access. Normally, we handle
  // messages coming from multiple RenderViews, but NaClProcessHost always
  // creates a new PepperMessageFilter for each RenderView.
  int nacl_render_view_id_;

  // When non-NULL, this should be used instead of the host_resolver_.
  ResourceContext* const resource_context_;

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
  TCPServerSocketMap tcp_server_sockets_;

  NetworkMonitorIdSet network_monitor_ids_;

  base::FilePath browser_path_;
  bool incognito_;

  DISALLOW_COPY_AND_ASSIGN(PepperMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_MESSAGE_FILTER_H_
