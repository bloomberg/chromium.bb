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

// Message filter that handles IPC for PPB_NetworkMonitor_Private and
// PPB_X509Certificate_Private.
class PepperMessageFilter
    : public BrowserMessageFilter,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  explicit PepperMessageFilter(const ppapi::PpapiPermissions& permissions);

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

  void OnNetworkMonitorStart(uint32 plugin_dispatcher_id);
  void OnNetworkMonitorStop(uint32 plugin_dispatcher_id);

  void OnX509CertificateParseDER(const std::vector<char>& der,
                                 bool* succeeded,
                                 ppapi::PPB_X509Certificate_Fields* result);

  void GetAndSendNetworkList();
  void DoGetNetworkList();
  void SendNetworkList(scoped_ptr<net::NetworkInterfaceList> list);

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
