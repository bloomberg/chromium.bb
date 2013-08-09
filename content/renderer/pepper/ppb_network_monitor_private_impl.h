// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_NETWORK_MONITOR_PRIVATE_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_NETWORK_MONITOR_PRIVATE_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/renderer/p2p/network_list_observer.h"
#include "ppapi/c/private/ppb_network_monitor_private.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_network_monitor_private_api.h"

namespace content {

class PPB_NetworkMonitor_Private_Impl
    : public ppapi::Resource,
      public ppapi::thunk::PPB_NetworkMonitor_Private_API,
      public content::NetworkListObserver {
 public:
  static PP_Resource Create(PP_Instance instance,
                            PPB_NetworkMonitor_Callback callback,
                            void* user_data);

  virtual ppapi::thunk::PPB_NetworkMonitor_Private_API*
      AsPPB_NetworkMonitor_Private_API() OVERRIDE;

  // NetworkListObserver interface.
  virtual void OnNetworkListChanged(
      const net::NetworkInterfaceList& list) OVERRIDE;

 private:
  PPB_NetworkMonitor_Private_Impl(PP_Instance instance,
                                  PPB_NetworkMonitor_Callback callback,
                                  void* user_data);
  virtual ~PPB_NetworkMonitor_Private_Impl();

  bool Start();

  PPB_NetworkMonitor_Callback callback_;
  void* user_data_;
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(PPB_NetworkMonitor_Private_Impl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_NETWORK_MONITOR_PRIVATE_IMPL_H_
