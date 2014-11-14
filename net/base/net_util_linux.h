// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_UTIL_LINUX_H_
#define NET_BASE_NET_UTIL_LINUX_H_

// This file is only used to expose some of the internals
// of net_util_linux.cc to tests.

#include "base/containers/hash_tables.h"
#include "net/base/address_tracker_linux.h"
#include "net/base/net_util.h"

namespace net {
namespace internal {

typedef char* (*GetInterfaceNameFunction)(unsigned int interface_index,
                                          char* ifname);

NET_EXPORT bool GetNetworkListImpl(
    NetworkInterfaceList* networks,
    int policy,
    const base::hash_set<int>& online_links,
    const internal::AddressTrackerLinux::AddressMap& address_map,
    GetInterfaceNameFunction get_interface_name);

}  // namespace internal
}  // namespace net

#endif  // NET_BASE_NET_UTIL_LINUX_H_
