// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_binding_util.h"

#include "base/logging.h"
#include "build/build_config.h"

namespace extensions {
namespace binding {

std::string GetPlatformString() {
#if defined(OS_CHROMEOS)
  return "chromeos";
#elif defined(OS_LINUX)
  return "linux";
#elif defined(OS_MACOSX)
  return "mac";
#elif defined(OS_WIN)
  return "win";
#else
  NOTREACHED();
  return std::string();
#endif
}

}  // namespace binding
}  // namespace extensions
