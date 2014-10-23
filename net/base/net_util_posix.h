// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_UTIL_POSIX_H_
#define NET_BASE_NET_UTIL_POSIX_H_

// This file is only used to expose some of the internals
// of net_util_posix.cc to tests.

namespace net {
namespace internal {

#if !defined(OS_MACOSX) && !defined(OS_NACL)
typedef char* (*GetInterfaceNameFunction)(unsigned int interface_index,
                                          char* ifname);

NET_EXPORT bool GetNetworkListImpl(
    NetworkInterfaceList* networks,
    int policy,
    const base::hash_set<int>& online_links,
    const internal::AddressTrackerLinux::AddressMap& address_map,
    GetInterfaceNameFunction get_interface_name);

#endif  // !OS_MACOSX && !OS_NACL

}  // namespace internal

}  // namespace net

#endif  // NET_BASE_NET_UTIL_POSIX_H_
