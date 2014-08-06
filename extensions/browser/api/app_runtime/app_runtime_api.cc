// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/app_runtime/app_runtime_api.h"

#include "base/time/time.h"
#include "base/values.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/granted_file_entry.h"
#include "extensions/common/api/app_runtime.h"
#include "url/gurl.h"

using content::BrowserContext;

namespace extensions {

namespace app_runtime = core_api::app_runtime;

namespace {

void DispatchOnEmbedRequestedEventImpl(
    const std::string& extension_id,
    scoped_ptr<base::DictionaryValue> app_embedding_request_data,
    content::BrowserContext* context) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(app_embedding_request_data.release());
  ExtensionSystem* system = ExtensionSystem::Get(context);
  scoped_ptr<Event> event(
      new Event(app_runtime::OnEmbedRequested::kEventName, args.Pass()));
  event->restrict_to_browser_context = context;
  system->event_router()->DispatchEventWithLazyListener(extension_id,
                                                        event.Pass());

  ExtensionPrefs::Get(context)
      ->SetLastLaunchTime(extension_id, base::Time::Now());
}

void DispatchOnLaunchedEventImpl(const std::string& extension_id,
                                 scoped_ptr<base::DictionaryValue> launch_data,
                                 BrowserContext* context) {
  // "Forced app mode" is true for Chrome OS kiosk mode.
  launch_data->SetBoolean(
      "isKioskSession",
      ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode());
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(launch_data.release());
  scoped_ptr<Event> event(
      new Event(app_runtime::OnLaunched::kEventName, args.Pass()));
  event->restrict_to_browser_context = context;
  EventRouter::Get(context)
      ->DispatchEventWithLazyListener(extension_id, event.Pass());
  ExtensionPrefs::Get(context)
      ->SetLastLaunchTime(extension_id, base::Time::Now());
}

}  // namespace

// static
void AppRuntimeEventRouter::DispatchOnEmbedRequestedEvent(
    content::BrowserContext* context,
    scoped_ptr<base::DictionaryValue> embed_app_data,
    const Extension* extension) {
  DispatchOnEmbedRequestedEventImpl(
      extension->id(), embed_app_data.Pass(), context);
}

// static
void AppRuntimeEventRouter::DispatchOnLaunchedEvent(
    BrowserContext* context,
    const Extension* extension) {
  scoped_ptr<base::DictionaryValue> launch_data(new base::DictionaryValue());
  DispatchOnLaunchedEventImpl(extension->id(), launch_data.Pass(), context);
}

// static
void AppRuntimeEventRouter::DispatchOnRestartedEvent(
    BrowserContext* context,
    const Extension* extension) {
  scoped_ptr<base::ListValue> arguments(new base::ListValue());
  scoped_ptr<Event> event(
      new Event(app_runtime::OnRestarted::kEventName, arguments.Pass()));
  event->restrict_to_browser_context = context;
  EventRouter::Get(context)
      ->DispatchEventToExtension(extension->id(), event.Pass());
}

// static
void AppRuntimeEventRouter::DispatchOnLaunchedEventWithFileEntries(
    BrowserContext* context,
    const Extension* extension,
    const std::string& handler_id,
    const std::vector<std::string>& mime_types,
    const std::vector<GrantedFileEntry>& file_entries) {
  // TODO(sergeygs): Use the same way of creating an event (using the generated
  // boilerplate) as below in DispatchOnLaunchedEventWithUrl.
  scoped_ptr<base::DictionaryValue> launch_data(new base::DictionaryValue);
  launch_data->SetString("id", handler_id);
  scoped_ptr<base::ListValue> items(new base::ListValue);
  DCHECK(file_entries.size() == mime_types.size());
  for (size_t i = 0; i < file_entries.size(); ++i) {
    scoped_ptr<base::DictionaryValue> launch_item(new base::DictionaryValue);
    launch_item->SetString("fileSystemId", file_entries[i].filesystem_id);
    launch_item->SetString("baseName", file_entries[i].registered_name);
    launch_item->SetString("mimeType", mime_types[i]);
    launch_item->SetString("entryId", file_entries[i].id);
    items->Append(launch_item.release());
  }
  launch_data->Set("items", items.release());
  DispatchOnLaunchedEventImpl(extension->id(), launch_data.Pass(), context);
}

// static
void AppRuntimeEventRouter::DispatchOnLaunchedEventWithUrl(
    BrowserContext* context,
    const Extension* extension,
    const std::string& handler_id,
    const GURL& url,
    const GURL& referrer_url) {
  app_runtime::LaunchData launch_data;
  launch_data.id.reset(new std::string(handler_id));
  launch_data.url.reset(new std::string(url.spec()));
  launch_data.referrer_url.reset(new std::string(referrer_url.spec()));
  DispatchOnLaunchedEventImpl(
      extension->id(), launch_data.ToValue().Pass(), context);
}

}  // namespace extensions
