// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_LOG_UTIL_H_
#define NET_BASE_NET_LOG_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace net {

class URLRequestContext;

// A set of flags that can be OR'd together to request specific information
// about the current state of the URLRequestContext.  See GetNetInfo, below.
enum NetInfoSource {
#define NET_INFO_SOURCE(label, string, value) NET_INFO_ ## label = value,
#include "net/base/net_info_source_list.h"
#undef NET_INFO_SOURCE
  NET_INFO_ALL_SOURCES = -1,
};

// Utility methods for creating NetLog dumps.

// Create a dictionary containing legend for net/ constants.
NET_EXPORT scoped_ptr<base::DictionaryValue> GetNetConstants();

// Retrieves a dictionary containing information about the current state of
// |context|.  |info_sources| is a set of NetInfoSources OR'd together,
// indicating just what information is being requested.  Each NetInfoSource adds
// one top-level entry to the returned dictionary.
NET_EXPORT scoped_ptr<base::DictionaryValue> GetNetInfo(
    URLRequestContext* context, int info_sources);

}  // namespace net

#endif  // NET_BASE_NET_LOG_UTIL_H_
