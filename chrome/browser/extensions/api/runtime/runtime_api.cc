// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/runtime/runtime_api.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/extension.h"
#include "googleurl/src/gurl.h"

namespace extensions {

namespace {

const char kOnStartupEvent[] = "runtime.onStartup";
const char kOnInstalledEvent[] = "runtime.onInstalled";
const char kNoBackgroundPageError[] = "You do not have a background page.";
const char kPageLoadError[] = "Background page failed to load.";
const char kInstallReason[] = "reason";
const char kInstallReasonUpdate[] = "update";
const char kInstallReasonInstall[] = "install";
const char kInstallPreviousVersion[] = "previousVersion";

static void DispatchOnStartupEventImpl(
    Profile* profile,
    const std::string& extension_id,
    bool first_call,
    ExtensionHost* host) {
  // A NULL host from the LazyBackgroundTaskQueue means the page failed to
  // load. Give up.
  if (!host && !first_call)
    return;

  if (g_browser_process->IsShuttingDown() ||
      !g_browser_process->profile_manager()->IsValidProfile(profile))
    return;
  ExtensionSystem* system = ExtensionSystem::Get(profile);
  if (!system)
    return;

  // If this is a persistent background page, we want to wait for it to load
  // (it might not be ready, since this is startup). But only enqueue once.
  // If it fails to load the first time, don't bother trying again.
  const Extension* extension =
      system->extension_service()->extensions()->GetByID(extension_id);
  if (extension && extension->has_persistent_background_page() && first_call &&
      system->lazy_background_task_queue()->
          ShouldEnqueueTask(profile, extension)) {
    system->lazy_background_task_queue()->AddPendingTask(
        profile, extension_id,
        base::Bind(&DispatchOnStartupEventImpl,
                   profile, extension_id, false));
    return;
  }

  scoped_ptr<base::ListValue> event_args(new ListValue());
  system->event_router()->DispatchEventToExtension(
      extension_id, kOnStartupEvent, event_args.Pass(), NULL, GURL());
}

}  // namespace

// static
void RuntimeEventRouter::DispatchOnStartupEvent(
    Profile* profile, const std::string& extension_id) {
  DispatchOnStartupEventImpl(profile, extension_id, true, NULL);
}

// static
void RuntimeEventRouter::DispatchOnInstalledEvent(
    Profile* profile,
    const std::string& extension_id,
    const Version& old_version) {
  ExtensionSystem* system = ExtensionSystem::Get(profile);
  if (!system)
    return;

  // Special case: normally, extensions add their own lazy event listeners.
  // However, since the extension has just been installed, it hasn't had a
  // chance to register for events. So we register on its behalf. If the
  // extension does not actually have a listener, the event will just be
  // ignored.
  scoped_ptr<base::ListValue> event_args(new ListValue());
  base::DictionaryValue* info = new base::DictionaryValue();
  event_args->Append(info);
  info->SetString(kInstallReason,
      old_version.IsValid() ? kInstallReasonUpdate : kInstallReasonInstall);
  if (old_version.IsValid())
    info->SetString(kInstallPreviousVersion, old_version.GetString());
  system->event_router()->AddLazyEventListener(kOnInstalledEvent, extension_id);
  system->event_router()->DispatchEventToExtension(
      extension_id, kOnInstalledEvent, event_args.Pass(), NULL, GURL());
  system->event_router()->RemoveLazyEventListener(kOnInstalledEvent,
                                                  extension_id);
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
