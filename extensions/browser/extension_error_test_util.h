// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_ERROR_TEST_UTIL_H_
#define EXTENSIONS_BROWSER_EXTENSION_ERROR_TEST_UTIL_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace extensions {

class ExtensionError;

namespace error_test_util {

// Create a new RuntimeError.
scoped_ptr<ExtensionError> CreateNewRuntimeError(
    const std::string& extension_id,
    const std::string& message,
    bool from_incognito);

// Create a new RuntimeError; incognito defaults to "false".
scoped_ptr<ExtensionError> CreateNewRuntimeError(
    const std::string& extension_id, const std::string& message);

// Create a new ManifestError.
scoped_ptr<ExtensionError> CreateNewManifestError(
    const std::string& extension_id, const std::string& mnessage);

}  // namespace error_test_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_ERROR_TEST_UTIL_H_
