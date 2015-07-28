// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/capability_filter.h"

namespace mojo {
namespace shell {

CapabilityFilter GetPermissiveCapabilityFilter() {
  CapabilityFilter filter;
  AllowedInterfaces interfaces;
  interfaces.insert("*");
  filter["*"] = interfaces;
  return filter;
}

}  // namespace shell
}  // namespace mojo
