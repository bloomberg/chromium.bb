// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_TEST_UTIL_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_TEST_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "extensions/common/manifest.h"

namespace extensions {
class Extension;
}

// Newer functions go in extensions/common/test_util.h.
namespace extension_test_util {

// Helpers for loading manifests, |dir| is relative to chrome::DIR_TEST_DATA
// followed by "extensions".
scoped_refptr<extensions::Extension> LoadManifestUnchecked(
    const std::string& dir,
    const std::string& test_file,
    extensions::Manifest::Location location,
    int extra_flags,
    const std::string& id,
    std::string* error);

scoped_refptr<extensions::Extension> LoadManifestUnchecked(
    const std::string& dir,
    const std::string& test_file,
    extensions::Manifest::Location location,
    int extra_flags,
    std::string* error);

scoped_refptr<extensions::Extension> LoadManifest(
    const std::string& dir,
    const std::string& test_file,
    extensions::Manifest::Location location,
    int extra_flags);

scoped_refptr<extensions::Extension> LoadManifest(const std::string& dir,
                                                  const std::string& test_file,
                                                  int extra_flags);

scoped_refptr<extensions::Extension> LoadManifestStrict(
    const std::string& dir,
    const std::string& test_file);

scoped_refptr<extensions::Extension> LoadManifest(const std::string& dir,
                                                  const std::string& test_file);

}  // namespace extension_test_util

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_TEST_UTIL_H_
