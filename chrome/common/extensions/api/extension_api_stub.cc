// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stub methods to be used when extensions are disabled
// i.e. ENABLE_EXTENSIONS is not defined

#include "chrome/common/extensions/api/extension_api.h"

namespace extensions {

// static
ExtensionAPI* ExtensionAPI::GetSharedInstance() {
  return NULL;
}

// static
ExtensionAPI* ExtensionAPI::CreateWithDefaultConfiguration() {
  return NULL;
}

bool ExtensionAPI::IsPrivileged(const std::string& full_name) {
  return false;
}

}  // namespace extensions
