// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_CATALOG_TYPES_H_
#define MOJO_SERVICES_CATALOG_TYPES_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"

namespace catalog {

class Entry;

// A map of mojo names -> catalog |Entry|s.
using EntryCache = std::map<std::string, scoped_ptr<Entry>>;

}  // namespace catalog

#endif  // MOJO_SERVICES_CATALOG_TYPES_H_
