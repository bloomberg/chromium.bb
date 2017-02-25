// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/extension_info.h"

#include "extensions/common/extension.h"

namespace safe_browsing {

ExtensionInfo::ExtensionInfo(const extensions::Extension* extension) {
  DCHECK(extension);
  id = extension->id();
  name = extension->name();
}

ExtensionInfo::ExtensionInfo(const extensions::ExtensionId& id,
                             const std::string& name)
    : id(id), name(name) {}

ExtensionInfo::~ExtensionInfo() {}

}  // namespace safe_browsing
