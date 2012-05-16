// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/runtime/runtime_api.h"

#include "chrome/common/extensions/extension.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/profiles/profile.h"
#include "googleurl/src/gurl.h"

namespace {

const char kOnInstalledEvent[] = "runtime.onInstalled";
const char kNoBackgroundPageError[] = "You do not have a background page.";
const char kPageLoadError[] = "Background page failed to load.";

}

namespace extensions {

// static
void RuntimeEventRouter::DispatchOnInstalledEvent(
    Profile* profile, const std::string& extension_id) {
  ExtensionSystem* system = ExtensionSystem::Get(profile);
  if (!system)
    return;

  // Special case: normally, extensions add their own lazy event listeners.
  // However, since the extension has just been installed, it hasn't had a
  // chance to register for events. So we register on its behalf. If the
  // extension does not actually have a listener, the event will just be
  // ignored.
  system->event_router()->AddLazyEventListener(kOnInstalledEvent, extension_id);
  system->event_router()->DispatchEventToExtension(
      extension_id, kOnInstalledEvent, "[]", NULL, GURL());
}

bool RuntimeGetBackgroundPageFunction::RunImpl() {
  ExtensionHost* host =
      ExtensionSystem::Get(profile())->process_manager()->
          GetBackgroundHostForExtension(extension_id());
  if (host) {
    OnPageLoaded(host);
  } else if (GetExtension()->has_lazy_background_page()) {
    ExtensionSystem::Get(profile())->lazy_background_task_queue()->
        AddPendingTask(
           profile(), extension_id(),
           base::Bind(&RuntimeGetBackgroundPageFunction::OnPageLoaded,
                      this));
  } else {
    error_ = kNoBackgroundPageError;
    return false;
  }

  return true;
}

void RuntimeGetBackgroundPageFunction::OnPageLoaded(ExtensionHost* host) {
  if (host) {
    SendResponse(true);
  } else {
    error_ = kPageLoadError;
    SendResponse(false);
  }
}

}   // namespace extensions
