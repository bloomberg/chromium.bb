// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_page_tracker.h"

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/background_application_list_model.h"
#include "chrome/browser/background_contents_service.h"
#include "chrome/browser/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"

///////////////////////////////////////////////////////////////////////////////
// BackgroundPageTracker keeps a single DictionaryValue (stored at
// prefs::kKnownBackgroundPages). We keep only two pieces of information for
// each background page: the parent application/extension ID, and a boolean
// flag that is true if the user has acknowledged this page.
//
// kKnownBackgroundPages:
//    DictionaryValue {
//       <appid_1>: false,
//       <appid_2>: true,
//         ... etc ...
//    }

// static
void BackgroundPageTracker::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kKnownBackgroundPages);
}

// static
BackgroundPageTracker* BackgroundPageTracker::GetInstance() {
  return Singleton<BackgroundPageTracker>::get();
}

int BackgroundPageTracker::GetBackgroundPageCount() {
  if (!IsEnabled())
    return 0;

  PrefService* prefs = GetPrefService();
  const DictionaryValue* contents =
      prefs->GetDictionary(prefs::kKnownBackgroundPages);
  return contents ? contents->size() : 0;
}

int BackgroundPageTracker::GetUnacknowledgedBackgroundPageCount() {
  if (!IsEnabled())
    return 0;
  PrefService* prefs = GetPrefService();
  const DictionaryValue* contents =
      prefs->GetDictionary(prefs::kKnownBackgroundPages);
  if (!contents)
    return 0;
  int count = 0;
  for (DictionaryValue::key_iterator it = contents->begin_keys();
       it != contents->end_keys(); ++it) {
    Value* value;
    bool found = contents->GetWithoutPathExpansion(*it, &value);
    DCHECK(found);
    bool acknowledged = true;
    bool valid = value->GetAsBoolean(&acknowledged);
    DCHECK(valid);
    if (!acknowledged)
      count++;
  }
  return count;
}

void BackgroundPageTracker::AcknowledgeBackgroundPages() {
  if (!IsEnabled())
    return;
  PrefService* prefs = GetPrefService();
  DictionaryValue* contents =
      prefs->GetMutableDictionary(prefs::kKnownBackgroundPages);
  if (!contents)
    return;
  bool prefs_modified = false;
  for (DictionaryValue::key_iterator it = contents->begin_keys();
       it != contents->end_keys(); ++it) {
    contents->SetWithoutPathExpansion(*it, Value::CreateBooleanValue(true));
    prefs_modified = true;
  }
  if (prefs_modified) {
    prefs->ScheduleSavePersistentPrefs();
    SendChangeNotification();
  }
}

BackgroundPageTracker::BackgroundPageTracker() {
  // If background mode is disabled, just exit - don't load information from
  // prefs or listen for any notifications so we will act as if there are no
  // background pages, effectively disabling any associated badging.
  if (!IsEnabled())
    return;

  // Check to make sure all of the extensions are loaded - once they are loaded
  // we can update the list.
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile();
  if (profile->GetExtensionService() &&
      profile->GetExtensionService()->is_ready()) {
    UpdateExtensionList();
    // We do not send any change notifications here, because the object was
    // just created (it doesn't seem appropriate to send a change notification
    // at initialization time). Also, since this is a singleton object, sending
    // a notification in the constructor can lead to deadlock if one of the
    // observers tries to get the singleton.
  } else {
    // Extensions aren't loaded yet - register to be notified when they are
    // ready.
    registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                   NotificationService::AllSources());
  }
}

BackgroundPageTracker::~BackgroundPageTracker() {
}

PrefService* BackgroundPageTracker::GetPrefService() {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  return service;
}

bool BackgroundPageTracker::IsEnabled() {
  // Disable the background page tracker for unittests.
  if (!g_browser_process->local_state())
    return false;

  // BackgroundPageTracker is enabled if background mode is enabled.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return BackgroundModeManager::IsBackgroundModeEnabled(command_line);
}

void BackgroundPageTracker::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSIONS_READY:
      if (UpdateExtensionList())
        SendChangeNotification();
      break;
    case NotificationType::BACKGROUND_CONTENTS_OPENED: {
      std::string id = UTF16ToUTF8(
          Details<BackgroundContentsOpenedDetails>(details)->application_id);
      OnBackgroundPageLoaded(id);
      break;
    }
    case NotificationType::EXTENSION_LOADED: {
      const Extension* extension = Details<const Extension>(details).ptr();
      if (extension->background_url().is_valid())
        OnBackgroundPageLoaded(extension->id());
      break;
    }
    case NotificationType::EXTENSION_UNLOADED: {
      std::string id = Details<UnloadedExtensionInfo>(details)->extension->id();
      OnExtensionUnloaded(id);
      break;
    }
    default:
      NOTREACHED();
  }
}

bool BackgroundPageTracker::UpdateExtensionList() {
  // Extensions are loaded - update our list.
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile();
  ExtensionService* extensions_service = profile->GetExtensionService();
  DCHECK(extensions_service);

  // We will make two passes to update the list:
  // 1) Walk our list, and make sure that there's a corresponding extension for
  //    each item in the list. If not, delete it (extension was uninstalled).
  // 2) Walk the set of currently loaded extensions and background contents, and
  //    make sure there's an entry in our list for each one. If not, create one.

  PrefService* prefs = GetPrefService();
  std::set<std::string> keys_to_delete;
  bool pref_modified = false;
  // If we've never set any prefs, then this is the first launch ever, so we
  // want to automatically mark all existing extensions as acknowledged.
  bool first_launch =
      prefs->GetDictionary(prefs::kKnownBackgroundPages) == NULL;
  DictionaryValue* contents =
      prefs->GetMutableDictionary(prefs::kKnownBackgroundPages);
  for (DictionaryValue::key_iterator it = contents->begin_keys();
       it != contents->end_keys(); ++it) {
    // Check to make sure that the parent extension is still enabled.
    const Extension* extension = extensions_service->GetExtensionById(
        *it, false);
    // If the extension is not loaded, add the id to our list of keys to delete
    // later (can't delete now since we're still iterating).
    if (!extension) {
      keys_to_delete.insert(*it);
      pref_modified = true;
    }
  }

  for (std::set<std::string>::const_iterator iter = keys_to_delete.begin();
       iter != keys_to_delete.end();
       ++iter) {
    contents->RemoveWithoutPathExpansion(*iter, NULL);
  }

  // Look for new extensions/background contents.
  const ExtensionList* list = extensions_service->extensions();
  for (ExtensionList::const_iterator iter = list->begin();
       iter != list->begin();
       ++iter) {
    // Any extension with a background page should be in our list.
    if ((*iter)->background_url().is_valid()) {
      // If we have not seen this extension ID before, add it to our list.
      if (!contents->HasKey((*iter)->id())) {
        contents->SetWithoutPathExpansion(
            (*iter)->id(), Value::CreateBooleanValue(first_launch));
        pref_modified = true;
      }
    }
  }

  // Add all apps with background contents also.
  BackgroundContentsService* background_contents_service =
      profile->GetBackgroundContentsService();
  std::vector<BackgroundContents*> background_contents =
      background_contents_service->GetBackgroundContents();
  for (std::vector<BackgroundContents*>::const_iterator iter =
           background_contents.begin();
       iter != background_contents.end();
       ++iter) {
     std::string application_id = UTF16ToUTF8(
         background_contents_service->GetParentApplicationId(*iter));
     if (!contents->HasKey(application_id)) {
        contents->SetWithoutPathExpansion(
            application_id, Value::CreateBooleanValue(first_launch));
        pref_modified = true;
     }
  }

  // Register for when new pages are loaded/unloaded so we can update our list.
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::BACKGROUND_CONTENTS_OPENED,
                 NotificationService::AllSources());

  // If we modified the list, save it to prefs and let our caller know.
  if (pref_modified)
    prefs->ScheduleSavePersistentPrefs();
  return pref_modified;
}

void BackgroundPageTracker::OnBackgroundPageLoaded(const std::string& id) {
  DCHECK(IsEnabled());
  PrefService* prefs = GetPrefService();
  DictionaryValue* contents =
      prefs->GetMutableDictionary(prefs::kKnownBackgroundPages);
  // No need to update our list if this extension was already known.
  if (!contents || contents->HasKey(id))
    return;

  // Update our list with this new as-yet-unacknowledged page.
  contents->SetWithoutPathExpansion(id, Value::CreateBooleanValue(false));
  prefs->ScheduleSavePersistentPrefs();
  SendChangeNotification();
}

void BackgroundPageTracker::OnExtensionUnloaded(const std::string& id) {
  DCHECK(IsEnabled());
  PrefService* prefs = GetPrefService();
  DictionaryValue* contents =
      prefs->GetMutableDictionary(prefs::kKnownBackgroundPages);

  if (!contents || !contents->HasKey(id))
    return;

  contents->RemoveWithoutPathExpansion(id, NULL);
  prefs->ScheduleSavePersistentPrefs();
  SendChangeNotification();
}

void BackgroundPageTracker::SendChangeNotification() {
  NotificationService::current()->Notify(
      NotificationType::BACKGROUND_PAGE_TRACKER_CHANGED,
      Source<BackgroundPageTracker>(this),
      NotificationService::NoDetails());
}
