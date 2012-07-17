// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_TEST_UTIL_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_TEST_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"

namespace extensions {
class Extension;
}

namespace extension_test_util {

// Makes a fake extension id using the given |seed|.
std::string MakeId(std::string seed);

// Return a very simple extension with id |id|.
scoped_refptr<extensions::Extension> CreateExtensionWithID(std::string id);

}  // namespace extension_test_util

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_TEST_UTIL_H_
