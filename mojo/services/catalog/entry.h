// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_CATALOG_ENTRY_H_
#define MOJO_SERVICES_CATALOG_ENTRY_H_

#include <map>
#include <set>
#include <string>

namespace catalog {

// A set of names of interfaces that may be exposed to an application.
using AllowedInterfaces = std::set<std::string>;
// A map of allowed applications to allowed interface sets. See shell.mojom for
// more details.
using CapabilityFilter = std::map<std::string, AllowedInterfaces>;

// Static information about an application package known to the Catalog.
struct Entry {
  Entry();
  Entry(const Entry& other);
  ~Entry();

  std::string name;
  std::string qualifier;
  std::string display_name;
  CapabilityFilter capabilities;
};

}  // namespace catalog

#endif  // MOJO_SERVICES_CATALOG_ENTRY_H_
