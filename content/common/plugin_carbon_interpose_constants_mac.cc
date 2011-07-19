// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__LP64__)

#include "content/common/plugin_carbon_interpose_constants_mac.h"

namespace plugin_interpose_strings {

const char kDYLDInsertLibrariesKey[] = "DYLD_INSERT_LIBRARIES";
const char kInterposeLibraryPath[] =
    "@executable_path/libplugin_carbon_interpose.dylib";

}  // namespace plugin_interpose_strings

#endif  // !__LP64__
