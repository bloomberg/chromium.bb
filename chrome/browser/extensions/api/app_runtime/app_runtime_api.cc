// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/app_runtime/app_runtime_api.h"

#include "base/json/json_writer.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"

namespace extensions {

namespace {

using event_names::kOnLaunched;
using event_names::kOnRestarted;

void DispatchOnLaunchedEventImpl(const std::string& extension_id,
                                 scoped_ptr<base::ListValue> args,
                                 Profile* profile) {
  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile);
  // Special case: normally, extensions add their own lazy event listeners.
  // However, since the extension might have just been enabled, it hasn't had a
  // chance to register for events. So we register on its behalf. If the
  // extension does not actually have a listener, the event will just be
  // ignored (but an app that doesn't listen for the onLaunched event doesn't
  // make sense anyway).
  system->event_router()->AddLazyEventListener(kOnLaunched, extension_id);
  scoped_ptr<Event> event(new Event(kOnLaunched, args.Pass()));
  event->restrict_to_profile = profile;
  system->event_router()->DispatchEventToExtension(extension_id, event.Pass());
  system->event_router()->RemoveLazyEventListener(kOnLaunched, extension_id);
}

}  // anonymous namespace

// static.
void AppEventRouter::DispatchOnLaunchedEvent(
    Profile* profile, const Extension* extension) {
  scoped_ptr<base::ListValue> arguments(new base::ListValue());
  DispatchOnLaunchedEventImpl(extension->id(), arguments.Pass(), profile);
}

// static.
void AppEventRouter::DispatchOnRestartedEvent(Profile* profile,
                                              const Extension* extension) {
  scoped_ptr<base::ListValue> arguments(new base::ListValue());
  scoped_ptr<Event> event(new Event(kOnRestarted, arguments.Pass()));
  event->restrict_to_profile = profile;
  extensions::ExtensionSystem::Get(profile)->event_router()->
      DispatchEventToExtension(extension->id(), event.Pass());
}

// static.
void AppEventRouter::DispatchOnLaunchedEventWithFileEntry(
    Profile* profile, const Extension* extension,
    const std::string& handler_id, const std::string& mime_type,
    const extensions::app_file_handler_util::GrantedFileEntry& file_entry) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  base::DictionaryValue* launch_data = new base::DictionaryValue();
  launch_data->SetString("id", handler_id);
  base::DictionaryValue* launch_item = new base::DictionaryValue;
  launch_item->SetString("fileSystemId", file_entry.filesystem_id);
  launch_item->SetString("baseName", file_entry.registered_name);
  launch_item->SetString("mimeType", mime_type);
  launch_item->SetString("entryId", file_entry.id);
  base::ListValue* items = new base::ListValue;
  items->Append(launch_item);
  launch_data->Set("items", items);
  args->Append(launch_data);
  DispatchOnLaunchedEventImpl(extension->id(), args.Pass(), profile);
}

}  // namespace extensions
