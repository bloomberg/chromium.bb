// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/runtime/runtime_api.h"

#include "chrome/common/extensions/extension.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "googleurl/src/gurl.h"

namespace {

const char kOnInstalledEvent[] = "experimental.runtime.onInstalled";

}

namespace extensions {

// static
void RuntimeEventRouter::DispatchOnInstalledEvent(
    Profile* profile, const Extension* extension) {
  // Special case: normally, extensions add their own lazy event listeners.
  // However, since the extension has just been installed, it hasn't had a
  // chance to register for events. So we register on its behalf. If the
  // extension does not actually have a listener, the event will just be
  // ignored.
  ExtensionEventRouter* router = profile->GetExtensionEventRouter();
  router->AddLazyEventListener(kOnInstalledEvent, extension->id());
  router->DispatchEventToExtension(
      extension->id(), kOnInstalledEvent, "[]", NULL, GURL());
}

}   // namespace extensions
