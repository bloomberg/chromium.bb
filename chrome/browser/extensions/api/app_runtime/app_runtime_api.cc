// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/app_runtime/app_runtime_api.h"

#include "base/json/json_writer.h"
#include "base/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
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
  scoped_ptr<ListValue> arguments(new ListValue());
  DispatchOnLaunchedEventImpl(extension->id(), arguments.Pass(), profile);
}

DictionaryValue* DictionaryFromSavedFileEntry(
    const app_file_handler_util::GrantedFileEntry& file_entry) {
  DictionaryValue* result = new DictionaryValue();
  result->SetString("id", file_entry.id);
  result->SetString("fileSystemId", file_entry.filesystem_id);
  result->SetString("baseName", file_entry.registered_name);
  return result;
}

// static.
void AppEventRouter::DispatchOnRestartedEvent(
    Profile* profile,
    const Extension* extension,
    const std::vector<app_file_handler_util::GrantedFileEntry>& file_entries) {
  ListValue* file_entries_list = new ListValue();
  for (std::vector<extensions::app_file_handler_util::GrantedFileEntry>
       ::const_iterator it = file_entries.begin(); it != file_entries.end();
       ++it) {
    file_entries_list->Append(DictionaryFromSavedFileEntry(*it));
  }
  scoped_ptr<ListValue> arguments(new ListValue());
  arguments->Append(file_entries_list);
  scoped_ptr<Event> event(new Event(kOnRestarted, arguments.Pass()));
  event->restrict_to_profile = profile;
  extensions::ExtensionSystem::Get(profile)->event_router()->
      DispatchEventToExtension(extension->id(), event.Pass());
}

// static.
void AppEventRouter::DispatchOnLaunchedEventWithFileEntry(
    Profile* profile, const Extension* extension,
    const std::string& handler_id, const std::string& mime_type,
    const std::string& file_system_id, const std::string& base_name) {
  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* launch_data = new DictionaryValue();
  launch_data->SetString("id", handler_id);
  DictionaryValue* launch_item = new DictionaryValue;
  launch_item->SetString("fileSystemId", file_system_id);
  launch_item->SetString("baseName", base_name);
  launch_item->SetString("mimeType", mime_type);
  ListValue* items = new ListValue;
  items->Append(launch_item);
  launch_data->Set("items", items);
  args->Append(launch_data);
  DispatchOnLaunchedEventImpl(extension->id(), args.Pass(), profile);
}

}  // namespace extensions
