// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_TEST_UTIL_H_
#define EXTENSIONS_COMMON_TEST_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace extensions {
class Extension;
class ExtensionBuilder;

namespace test_util {

// Adds an extension manifest to a builder.
ExtensionBuilder& BuildExtension(ExtensionBuilder& builder);

// Creates an extension instance that can be attached to an ExtensionFunction
// before running it.
scoped_refptr<Extension> CreateEmptyExtension();

// Return a very simple extension with a given |id|.
scoped_refptr<Extension> CreateExtensionWithID(const std::string& id);

// Parses |json| allowing trailing commas and replacing single quotes with
// double quotes for test readability. If the json fails to parse, calls gtest's
// ADD_FAILURE and returns an empty dictionary.
scoped_ptr<base::DictionaryValue> ParseJsonDictionaryWithSingleQuotes(
    std::string json);

}  // namespace test_util
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_TEST_UTIL_H_
