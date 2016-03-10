// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/catalog/entry.h"

namespace catalog {

Entry::Entry() {}
Entry::Entry(const Entry& other) = default;
Entry::~Entry() {}

bool Entry::operator==(const Entry& other) const {
  return other.name == name && other.qualifier == qualifier &&
         other.display_name == display_name &&
         other.capabilities == capabilities;
}

}  // catalog
