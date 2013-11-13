// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/app_runtime/app_runtime_api.h"

#include "base/json/json_writer.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/app_runtime.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

namespace extensions {

namespace app_runtime = api::app_runtime;

namespace {

void DispatchOnLaunchedEventImpl(const std::string& extension_id,
                                 scoped_ptr<base::DictionaryValue> launch_data,
                                 Profile* profile) {
#if defined(OS_CHROMEOS)
  launch_data->SetBoolean("isKioskSession",
                          chromeos::UserManager::Get()->IsLoggedInAsKioskApp());
#else
  launch_data->SetBoolean("isKioskSession", false);
#endif
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(launch_data.release());
  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile);
  scoped_ptr<Event> event(new Event(app_runtime::OnLaunched::kEventName,
                                    args.Pass()));
  event->restrict_to_browser_context = profile;
  system->event_router()->DispatchEventWithLazyListener(extension_id,
                                                        event.Pass());
  system->extension_service()->extension_prefs()->SetLastLaunchTime(
      extension_id, base::Time::Now());
}

}  // anonymous namespace

// static.
void AppEventRouter::DispatchOnLaunchedEvent(
    Profile* profile, const Extension* extension) {
  scoped_ptr<base::DictionaryValue> launch_data(new base::DictionaryValue());
  DispatchOnLaunchedEventImpl(extension->id(), launch_data.Pass(), profile);
}

// static.
void AppEventRouter::DispatchOnRestartedEvent(Profile* profile,
                                              const Extension* extension) {
  scoped_ptr<base::ListValue> arguments(new base::ListValue());
  scoped_ptr<Event> event(new Event(app_runtime::OnRestarted::kEventName,
                                    arguments.Pass()));
  event->restrict_to_browser_context = profile;
  extensions::ExtensionSystem::Get(profile)->event_router()->
      DispatchEventToExtension(extension->id(), event.Pass());
}

// static.
void AppEventRouter::DispatchOnLaunchedEventWithFileEntry(
    Profile* profile, const Extension* extension,
    const std::string& handler_id, const std::string& mime_type,
    const extensions::app_file_handler_util::GrantedFileEntry& file_entry) {
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
  DispatchOnLaunchedEventImpl(extension->id(), launch_data.Pass(), profile);
}

// static.
void AppEventRouter::DispatchOnLaunchedEventWithUrl(
    Profile* profile,
    const Extension* extension,
    const std::string& handler_id,
    const GURL& url,
    const GURL& referrer_url) {
  api::app_runtime::LaunchData launch_data;
  launch_data.id.reset(new std::string(handler_id));
  launch_data.url.reset(new std::string(url.spec()));
  launch_data.referrer_url.reset(new std::string(referrer_url.spec()));
  DispatchOnLaunchedEventImpl(
      extension->id(), launch_data.ToValue().Pass(), profile);
}

}  // namespace extensions
