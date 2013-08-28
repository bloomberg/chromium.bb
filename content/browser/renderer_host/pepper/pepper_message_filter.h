// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_MESSAGE_FILTER_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_message_filter.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

namespace ppapi {
class PPB_X509Certificate_Fields;
}

namespace content {

// This class is used in two contexts, both supporting PPAPI plugins. The first
// is on the renderer->browser channel, to handle requests from in-process
// PPAPI plugins and any requests that the PPAPI implementation code in the
// renderer needs to make. The second is on the plugin->browser channel to
// handle requests that out-of-process plugins send directly to the browser.
class PepperMessageFilter
    : public BrowserMessageFilter,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  // Factory method used in the context of a renderer process.
  static PepperMessageFilter* CreateForRendererProcess();

  // Factory method used in the context of a PPAPI process.
  static PepperMessageFilter* CreateForPpapiPluginProcess(
      const ppapi::PpapiPermissions& permissions);

  // Factory method used in the context of an external plugin,
  static PepperMessageFilter* CreateForExternalPluginProcess(
      const ppapi::PpapiPermissions& permissions);

  // BrowserMessageFilter methods.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // net::NetworkChangeNotifier::IPAddressObserver interface.
  virtual void OnIPAddressChanged() OVERRIDE;

 protected:
  virtual ~PepperMessageFilter();

 private:
  // Set of disptachers ID's that have subscribed for NetworkMonitor
  // notifications.
  typedef std::set<uint32> NetworkMonitorIdSet;

  enum PluginType {
    PLUGIN_TYPE_IN_PROCESS,
    PLUGIN_TYPE_OUT_OF_PROCESS,
    // External plugin means it was created through
    // BrowserPpapiHost::CreateExternalPluginProcess.
    PLUGIN_TYPE_EXTERNAL_PLUGIN,
  };

  PepperMessageFilter(const ppapi::PpapiPermissions& permissions,
                      PluginType plugin_type);

  void OnNetworkMonitorStart(uint32 plugin_dispatcher_id);
  void OnNetworkMonitorStop(uint32 plugin_dispatcher_id);

  void OnX509CertificateParseDER(const std::vector<char>& der,
                                 bool* succeeded,
                                 ppapi::PPB_X509Certificate_Fields* result);

  void GetAndSendNetworkList();
  void DoGetNetworkList();
  void SendNetworkList(scoped_ptr<net::NetworkInterfaceList> list);

  PluginType plugin_type_;

  // When attached to an out-of-process plugin (be it native or NaCl) this
  // will have the Pepper permissions for the plugin. When attached to the
  // renderer channel, this will have no permissions listed (since there may
  // be many plugins sharing this channel).
  ppapi::PpapiPermissions permissions_;

  NetworkMonitorIdSet network_monitor_ids_;

  DISALLOW_COPY_AND_ASSIGN(PepperMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_MESSAGE_FILTER_H_
