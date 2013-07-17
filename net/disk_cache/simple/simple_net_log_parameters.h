// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_NET_LOG_PARAMETERS_H_
#define NET_DISK_CACHE_SIMPLE_NET_LOG_PARAMETERS_H_

#include "net/base/net_log.h"

// This file augments the functions in net/disk_cache/net_log_parameters.h to
// include ones that deal with specifics of the Simple Cache backend.
namespace disk_cache {

class SimpleEntryImpl;

// Creates a NetLog callback that returns parameters for the creation of a
// SimpleEntryImpl. Contains the entry's key and hash, and whether it was
// created or opened. |entry| can't be NULL and must outlive the returned
// callback.
net::NetLog::ParametersCallback CreateNetLogSimpleEntryCreationCallback(
    const SimpleEntryImpl* entry);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_NET_LOG_PARAMETERS_H_
