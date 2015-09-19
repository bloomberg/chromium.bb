// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/capability_filter.h"

#include "mojo/shell/identity.h"

namespace mojo {
namespace shell {

CapabilityFilter GetPermissiveCapabilityFilter() {
  CapabilityFilter filter;
  AllowedInterfaces interfaces;
  interfaces.insert("*");
  filter["*"] = interfaces;
  return filter;
}

AllowedInterfaces GetAllowedInterfaces(const CapabilityFilter& filter,
                                       const Identity& identity) {
  // Start by looking for interfaces specific to the supplied identity.
  auto it = filter.find(identity.url().spec());
  if (it != filter.end())
    return it->second;

  // Fall back to looking for a wildcard rule.
  it = filter.find("*");
  if (filter.size() == 1 && it != filter.end())
    return it->second;

  // Finally, nothing is allowed.
  return AllowedInterfaces();
}

}  // namespace shell
}  // namespace mojo
