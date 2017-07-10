// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDING_API_BINDING_UTIL_H_
#define EXTENSIONS_RENDERER_BINDING_API_BINDING_UTIL_H_

#include <string>

namespace extensions {
namespace binding {

// Returns the string version of the current platform, one of "chromeos",
// "linux", "win", or "mac".
std::string GetPlatformString();

}  // namespace binding
}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDING_API_BINDING_UTIL_H_
