// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/browser/api/app_runtime/app_runtime_api.h"

#include "apps/browser/file_handler_util.h"
#include "apps/common/api/app_runtime.h"
#include "base/time/time.h"
#include "base/values.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "url/gurl.h"

using content::BrowserContext;
using extensions::Event;
using extensions::Extension;
using extensions::ExtensionPrefs;

namespace apps {

namespace app_runtime = api::app_runtime;

namespace {

void DispatchOnLaunchedEventImpl(const std::string& extension_id,
                                 scoped_ptr<base::DictionaryValue> launch_data,
                                 BrowserContext* context) {
  // "Forced app mode" is true for Chrome OS kiosk mode.
  launch_data->SetBoolean(
      "isKioskSession",
      extensions::ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode());
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(launch_data.release());
  scoped_ptr<Event> event(
      new Event(app_runtime::OnLaunched::kEventName, args.Pass()));
  event->restrict_to_browser_context = context;
  event->can_load_ephemeral_apps = true;
  extensions::EventRouter::Get(context)
      ->DispatchEventWithLazyListener(extension_id, event.Pass());
  ExtensionPrefs::Get(context)
      ->SetLastLaunchTime(extension_id, base::Time::Now());
}

}  // anonymous namespace

// static.
void AppEventRouter::DispatchOnLaunchedEvent(BrowserContext* context,
                                             const Extension* extension) {
  scoped_ptr<base::DictionaryValue> launch_data(new base::DictionaryValue());
  DispatchOnLaunchedEventImpl(extension->id(), launch_data.Pass(), context);
}

// static.
void AppEventRouter::DispatchOnRestartedEvent(BrowserContext* context,
                                              const Extension* extension) {
  scoped_ptr<base::ListValue> arguments(new base::ListValue());
  scoped_ptr<Event> event(
      new Event(app_runtime::OnRestarted::kEventName, arguments.Pass()));
  event->restrict_to_browser_context = context;
  event->can_load_ephemeral_apps = true;
  extensions::EventRouter::Get(context)
      ->DispatchEventToExtension(extension->id(), event.Pass());
}

// static.
void AppEventRouter::DispatchOnLaunchedEventWithFileEntry(
    BrowserContext* context,
    const Extension* extension,
    const std::string& handler_id,
    const std::string& mime_type,
    const file_handler_util::GrantedFileEntry& file_entry) {
  // TODO(sergeygs): Use the same way of creating an event (using the generated
  // boilerplate) as below in DispatchOnLaunchedEventWithUrl.
  scoped_ptr<base::DictionaryValue> launch_data(new base::DictionaryValue);
  launch_data->SetString("id", handler_id);
  scoped_ptr<base::DictionaryValue> launch_item(new base::DictionaryValue);
  launch_item->SetString("fileSystemId", file_entry.filesystem_id);
  launch_item->SetString("baseName", file_entry.registered_name);
  launch_item->SetString("mimeType", mime_type);
  launch_item->SetString("entryId", file_entry.id);
  scoped_ptr<base::ListValue> items(new base::ListValue);
  items->Append(launch_item.release());
  launch_data->Set("items", items.release());
  DispatchOnLaunchedEventImpl(extension->id(), launch_data.Pass(), context);
}

// static.
void AppEventRouter::DispatchOnLaunchedEventWithUrl(
    BrowserContext* context,
    const Extension* extension,
    const std::string& handler_id,
    const GURL& url,
    const GURL& referrer_url) {
  api::app_runtime::LaunchData launch_data;
  launch_data.id.reset(new std::string(handler_id));
  launch_data.url.reset(new std::string(url.spec()));
  launch_data.referrer_url.reset(new std::string(referrer_url.spec()));
  DispatchOnLaunchedEventImpl(
      extension->id(), launch_data.ToValue().Pass(), context);
}

}  // namespace apps
