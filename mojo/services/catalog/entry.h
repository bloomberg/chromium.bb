// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_CATALOG_ENTRY_H_
#define MOJO_SERVICES_CATALOG_ENTRY_H_

#include <string>

#include "mojo/shell/public/cpp/capabilities.h"

namespace catalog {

// Static information about an application package known to the Catalog.
struct Entry {
  Entry();
  Entry(const Entry& other);
  ~Entry();

  bool operator==(const Entry& other) const;

  std::string name;
  std::string qualifier;
  std::string display_name;
  mojo::CapabilitySpec capabilities;
};

}  // namespace catalog

#endif  // MOJO_SERVICES_CATALOG_ENTRY_H_
