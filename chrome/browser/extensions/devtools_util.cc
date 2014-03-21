// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/devtools_util.h"

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"

namespace extensions {
namespace devtools_util {

namespace {

// Helper to inspect an ExtensionHost after it has been loaded.
void InspectExtensionHost(ExtensionHost* host) {
  if (host)
    DevToolsWindow::OpenDevToolsWindow(host->render_view_host());
}

}  // namespace

void InspectBackgroundPage(const Extension* extension, Profile* profile) {
  DCHECK(extension);
  ExtensionSystem* system = ExtensionSystem::Get(profile);
  ExtensionHost* host =
      system->process_manager()->GetBackgroundHostForExtension(extension->id());
  if (host) {
    InspectExtensionHost(host);
  } else {
    system->lazy_background_task_queue()->AddPendingTask(
        profile,
        extension->id(),
        base::Bind(&InspectExtensionHost));
  }
}

}  // namespace devtools_util
}  // namespace extensions
