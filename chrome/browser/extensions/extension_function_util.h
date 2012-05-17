// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_UTIL_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_UTIL_H__
#pragma once

#include <vector>
#include "base/values.h"

namespace base {
class Value;
}

// This file contains various utility functions for extension API
// implementations.
namespace extensions {

// Reads the |value| as either a single integer value or a list of integers.
bool ReadOneOrMoreIntegers(base::Value* value, std::vector<int>* result);

} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_UTIL_H__
