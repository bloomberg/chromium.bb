// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_util.h"

#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {
namespace util {

bool IsExtensionInstalledPermanently(const std::string& extension_id,
                                     content::BrowserContext* context) {
  const Extension* extension = ExtensionRegistry::Get(context)->
      GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);
  return extension && !IsEphemeralApp(extension_id, context);
}

bool IsEphemeralApp(const std::string& extension_id,
                    content::BrowserContext* context) {
  return ExtensionPrefs::Get(context)->IsEphemeralApp(extension_id);
}

}  // namespace util
}  // namespace extensions
