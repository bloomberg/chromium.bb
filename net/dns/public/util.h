// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_UTIL_H_
#define NET_DNS_PUBLIC_UTIL_H_

#include <string>

#include "net/base/net_export.h"

namespace net {

// Basic utility functions for interaction with DNS, MDNS, and host resolution.
namespace dns_util {

// Returns true if the URI template is acceptable for sending requests via the
// given method. The template must be properly formatted, GET requests require
// the template to contain a "dns" variable, an expanded template must parse
// to a valid HTTPS URL, and the "dns" variable may not be part of the hostname.
NET_EXPORT bool IsValidDoHTemplate(const std::string& server_template,
                                   const std::string& server_method);

}  // namespace dns_util
}  // namespace net

#endif  // NET_DNS_PUBLIC_UTIL_H_
