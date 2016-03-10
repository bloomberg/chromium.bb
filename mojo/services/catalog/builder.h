// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_CATALOG_BUILDER_H_
#define MOJO_SERVICES_CATALOG_BUILDER_H_

#include "mojo/services/catalog/entry.h"

namespace base {
class DictionaryValue;
}

namespace catalog {

Entry BuildEntry(const base::DictionaryValue& value);

scoped_ptr<base::DictionaryValue> SerializeEntry(const Entry& entry);

}  // namespace catalog

#endif  // MOJO_SERVICES_CATALOG_BUILDER_H_
