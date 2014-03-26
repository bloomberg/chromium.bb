// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/runtime/runtime_event_router.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "base/version.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/common/manifest_handlers/background_info.h"

using content::BrowserContext;

namespace extensions {

namespace runtime = api::runtime;

namespace {

const char kInstallReason[] = "reason";
const char kInstallReasonChromeUpdate[] = "chrome_update";
const char kInstallReasonUpdate[] = "update";
const char kInstallReasonInstall[] = "install";
const char kInstallPreviousVersion[] = "previousVersion";

// Dispatches the onStartup event. May do so by posting a task if the extension
// background page has not yet loaded.
void DispatchOnStartupEventImpl(BrowserContext* browser_context,
                                const std::string& extension_id,
                                bool first_call,
                                ExtensionHost* host) {
  // A NULL host from the LazyBackgroundTaskQueue means the page failed to
  // load. Give up.
  if (!host && !first_call)
    return;

  // Don't send onStartup events to incognito browser contexts.
  if (browser_context->IsOffTheRecord())
    return;

  if (ExtensionsBrowserClient::Get()->IsShuttingDown() ||
      !ExtensionsBrowserClient::Get()->IsValidContext(browser_context))
    return;
  ExtensionSystem* system = ExtensionSystem::Get(browser_context);
  if (!system)
    return;

  // If this is a persistent background page, we want to wait for it to load
  // (it might not be ready, since this is startup). But only enqueue once.
  // If it fails to load the first time, don't bother trying again.
  const Extension* extension =
      ExtensionRegistry::Get(browser_context)
          ->GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
  LazyBackgroundTaskQueue* queue = system->lazy_background_task_queue();
  if (extension && BackgroundInfo::HasPersistentBackgroundPage(extension) &&
      first_call && queue->ShouldEnqueueTask(browser_context, extension)) {
    queue->AddPendingTask(
        browser_context,
        extension_id,
        base::Bind(
            &DispatchOnStartupEventImpl, browser_context, extension_id, false));
    return;
  }

  scoped_ptr<base::ListValue> event_args(new base::ListValue());
  scoped_ptr<Event> event(
      new Event(runtime::OnStartup::kEventName, event_args.Pass()));
  system->event_router()->DispatchEventToExtension(extension_id, event.Pass());
}

}  // namespace

// static
void RuntimeEventRouter::DispatchOnStartupEvent(
    BrowserContext* context,
    const std::string& extension_id) {
  DispatchOnStartupEventImpl(context, extension_id, true, NULL);
}

// static
void RuntimeEventRouter::DispatchOnInstalledEvent(
    BrowserContext* context,
    const std::string& extension_id,
    const base::Version& old_version,
    bool chrome_updated) {
  if (!ExtensionsBrowserClient::Get()->IsValidContext(context))
    return;
  ExtensionSystem* system = ExtensionSystem::Get(context);
  if (!system)
    return;

  scoped_ptr<base::ListValue> event_args(new base::ListValue());
  base::DictionaryValue* info = new base::DictionaryValue();
  event_args->Append(info);
  if (old_version.IsValid()) {
    info->SetString(kInstallReason, kInstallReasonUpdate);
    info->SetString(kInstallPreviousVersion, old_version.GetString());
  } else if (chrome_updated) {
    info->SetString(kInstallReason, kInstallReasonChromeUpdate);
  } else {
    info->SetString(kInstallReason, kInstallReasonInstall);
  }
  DCHECK(system->event_router());
  scoped_ptr<Event> event(
      new Event(runtime::OnInstalled::kEventName, event_args.Pass()));
  system->event_router()->DispatchEventWithLazyListener(extension_id,
                                                        event.Pass());
}

// static
void RuntimeEventRouter::DispatchOnUpdateAvailableEvent(
    BrowserContext* context,
    const std::string& extension_id,
    const base::DictionaryValue* manifest) {
  ExtensionSystem* system = ExtensionSystem::Get(context);
  if (!system)
    return;

  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(manifest->DeepCopy());
  DCHECK(system->event_router());
  scoped_ptr<Event> event(
      new Event(runtime::OnUpdateAvailable::kEventName, args.Pass()));
  system->event_router()->DispatchEventToExtension(extension_id, event.Pass());
}

// static
void RuntimeEventRouter::DispatchOnBrowserUpdateAvailableEvent(
    BrowserContext* context) {
  ExtensionSystem* system = ExtensionSystem::Get(context);
  if (!system)
    return;

  scoped_ptr<base::ListValue> args(new base::ListValue);
  DCHECK(system->event_router());
  scoped_ptr<Event> event(
      new Event(runtime::OnBrowserUpdateAvailable::kEventName, args.Pass()));
  system->event_router()->BroadcastEvent(event.Pass());
}

// static
void RuntimeEventRouter::DispatchOnRestartRequiredEvent(
    BrowserContext* context,
    const std::string& app_id,
    api::runtime::OnRestartRequired::Reason reason) {
  ExtensionSystem* system = ExtensionSystem::Get(context);
  if (!system)
    return;

  scoped_ptr<Event> event(
      new Event(runtime::OnRestartRequired::kEventName,
                api::runtime::OnRestartRequired::Create(reason)));

  DCHECK(system->event_router());
  system->event_router()->DispatchEventToExtension(app_id, event.Pass());
}

}  // namespace extensions
