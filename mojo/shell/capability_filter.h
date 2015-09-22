// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_CAPABILITY_FILTER_H_
#define MOJO_SHELL_CAPABILITY_FILTER_H_

#include <map>
#include <set>

#include "mojo/public/cpp/bindings/array.h"

namespace mojo {
namespace shell {

class Identity;

// A set of names of interfaces that may be exposed to an application.
using AllowedInterfaces = std::set<std::string>;
// A map of allowed applications to allowed interface sets. See shell.mojom for
// more details.
using CapabilityFilter = std::map<std::string, AllowedInterfaces>;

// Returns a capability filter that allows an application to connect to any
// other application and any service exposed by other applications.
CapabilityFilter GetPermissiveCapabilityFilter();

// Returns the set of interfaces that an application instance with |filter| is
// allowed to see from an instance with |identity|.
AllowedInterfaces GetAllowedInterfaces(const CapabilityFilter& filter,
                                       const Identity& identity);

}  // namespace shell
}  // namespace mojo


#endif  // MOJO_SHELL_CAPABILITY_FILTER_H_
