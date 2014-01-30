// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_system.h"

#include "chrome/browser/extensions/extension_system_factory.h"

namespace extensions {

ExtensionSystem::ExtensionSystem() {
}

ExtensionSystem::~ExtensionSystem() {
}

// static
ExtensionSystem* ExtensionSystem::Get(content::BrowserContext* context) {
  return ExtensionSystemFactory::GetForBrowserContext(context);
}

}  // namespace extensions
