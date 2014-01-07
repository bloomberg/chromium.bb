// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_TEST_UTIL_H_
#define EXTENSIONS_COMMON_TEST_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"

namespace extensions {
class Extension;
class ExtensionBuilder;

namespace test_util {

// Adds an extension manifest to a builder.
ExtensionBuilder& BuildExtension(ExtensionBuilder& builder);

// Return a very simple extension with a given |id|.
scoped_refptr<Extension> CreateExtensionWithID(const std::string& id);

}  // namespace test_util
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_TEST_UTIL_H_
