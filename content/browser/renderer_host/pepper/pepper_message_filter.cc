// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "content/browser/renderer_host/pepper/pepper_socket_utils.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "ppapi/c/private/ppb_network_list_private.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"

using ppapi::NetAddressPrivateImpl;

namespace content {

// static
PepperMessageFilter* PepperMessageFilter::CreateForRendererProcess() {
  return new PepperMessageFilter(ppapi::PpapiPermissions(),
                                 PLUGIN_TYPE_IN_PROCESS);
}

// static
PepperMessageFilter* PepperMessageFilter::CreateForPpapiPluginProcess(
    const ppapi::PpapiPermissions& permissions) {
  return new PepperMessageFilter(permissions,
                                 PLUGIN_TYPE_OUT_OF_PROCESS);
}

// static
PepperMessageFilter* PepperMessageFilter::CreateForExternalPluginProcess(
    const ppapi::PpapiPermissions& permissions) {
  return new PepperMessageFilter(permissions,
                                 PLUGIN_TYPE_EXTERNAL_PLUGIN);
}

bool PepperMessageFilter::OnMessageReceived(const IPC::Message& msg,
                                            bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PepperMessageFilter, msg, *message_was_ok)
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

PepperMessageFilter::PepperMessageFilter(
    const ppapi::PpapiPermissions& permissions,
    PluginType plugin_type)
    : plugin_type_(plugin_type),
      permissions_(permissions) {
}

PepperMessageFilter::~PepperMessageFilter() {
  if (!network_monitor_ids_.empty())
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
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
  *succeeded =
      pepper_socket_utils::GetCertificateFields(&der[0], der.size(), result);
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
