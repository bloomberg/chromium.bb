// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/shortcuts_backend.h"

#include <map>
#include <string>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/shortcuts_database.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/guid.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"

namespace history {

ShortcutsBackend::ShortcutsBackend(const FilePath& db_folder_path,
                                   Profile *profile)
    : current_state_(NOT_INITIALIZED),
      observer_list_(new ObserverListThreadSafe<ShortcutsBackendObserver>()),
      db_(db_folder_path) {
  // |profile| can be NULL in tests.
  if (profile) {
    notification_registrar_.Add(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                                Source<Profile>(profile));
    notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                                Source<Profile>(profile));
  }
}

ShortcutsBackend::~ShortcutsBackend() {}

bool ShortcutsBackend::Init() {
  if (base::subtle::NoBarrier_CompareAndSwap(&current_state_,
                                             NOT_INITIALIZED,
                                             INITIALIZING) == NOT_INITIALIZED) {
    return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        NewRunnableMethod(this, &ShortcutsBackend::InitInternal));
  } else {
    return false;
  }
}

bool ShortcutsBackend::AddShortcut(shortcuts_provider::Shortcut shortcut) {
  // It is safe to add a shortcut while the backend is being initialized since
  // the addition will occur on the same thread as the initialization.
  DCHECK(current_state_ != NOT_INITIALIZED);
  return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      NewRunnableMethod(this, &ShortcutsBackend::AddOrUpdateShortcutInternal,
                        shortcut, true));
}

bool ShortcutsBackend::UpdateShortcut(shortcuts_provider::Shortcut shortcut) {
  DCHECK(initialized());
  return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      NewRunnableMethod(this, &ShortcutsBackend::AddOrUpdateShortcutInternal,
                        shortcut, false));
}

bool ShortcutsBackend::DeleteShortcutsWithIds(
    const std::vector<std::string>& shortcut_ids) {
  DCHECK(initialized());
  return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      NewRunnableMethod(this, &ShortcutsBackend::DeleteShortcutsWithIdsInternal,
                        shortcut_ids));
}

bool ShortcutsBackend::DeleteShortcutsWithUrl(const GURL& shortcut_url) {
  DCHECK(initialized());
  return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      NewRunnableMethod(this, &ShortcutsBackend::DeleteShortcutsWithUrlInternal,
                        shortcut_url.spec()));
}

bool ShortcutsBackend::GetShortcuts(
    shortcuts_provider::ShortcutMap* shortcuts) {
  DCHECK(initialized());
  DCHECK(shortcuts);

  if (!initialized())
    return false;
  shortcuts->clear();
  base::AutoLock lock(data_access_lock_);
  shortcuts->insert(shortcuts_map_.begin(), shortcuts_map_.end());
  return true;
}

void ShortcutsBackend::InitInternal() {
  db_.Init();
  shortcuts_provider::GuidToShortcutMap shortcuts;
  db_.LoadShortcuts(&shortcuts);
  for (shortcuts_provider::GuidToShortcutMap::iterator it = shortcuts.begin();
       it != shortcuts.end(); ++it) {
    guid_map_[it->first] = shortcuts_map_.insert(
        std::make_pair(base::i18n::ToLower(it->second.text), it->second));
  }
  base::subtle::NoBarrier_CompareAndSwap(&current_state_, INITIALIZING,
                                         INITIALIZED);
  observer_list_->Notify(&ShortcutsBackendObserver::OnShortcutsLoaded);
}

void ShortcutsBackend::AddOrUpdateShortcutInternal(
    shortcuts_provider::Shortcut shortcut, bool add) {
  {
    // Update local copy.
    base::AutoLock lock(data_access_lock_);
    shortcuts_provider::GuidToShortcutsIteratorMap::iterator it =
        guid_map_.find(shortcut.id);
    if (it != guid_map_.end())
      shortcuts_map_.erase(it->second);
    guid_map_[shortcut.id] = shortcuts_map_.insert(
        std::make_pair(base::i18n::ToLower(shortcut.text), shortcut));
  }
  if (add)
    db_.AddShortcut(shortcut);
  else
    db_.UpdateShortcut(shortcut);
  observer_list_->Notify(&ShortcutsBackendObserver::OnShortcutAddedOrUpdated,
                         shortcut);
}

void ShortcutsBackend::DeleteShortcutsWithIdsInternal(
    std::vector<std::string> shortcut_ids) {
  {
    // Update local copy.
    base::AutoLock lock(data_access_lock_);
    for (size_t i = 0; i < shortcut_ids.size(); ++i) {
      shortcuts_provider::GuidToShortcutsIteratorMap::iterator it =
          guid_map_.find(shortcut_ids[i]);
      if (it != guid_map_.end()) {
        shortcuts_map_.erase(it->second);
        guid_map_.erase(it);
      }
    }
  }
  db_.DeleteShortcutsWithIds(shortcut_ids);
  observer_list_->Notify(&ShortcutsBackendObserver::OnShortcutsRemoved,
                         shortcut_ids);
}

void ShortcutsBackend::DeleteShortcutsWithUrlInternal(
    std::string shortcut_url) {
  std::vector<std::string> shortcut_ids;
  {
    // Update local copy.
    base::AutoLock lock(data_access_lock_);
    for (shortcuts_provider::GuidToShortcutsIteratorMap::iterator
             it = guid_map_.begin();
         it != guid_map_.end();) {
      if (it->second->second.url.spec() == shortcut_url) {
        shortcut_ids.push_back(it->first);
        shortcuts_map_.erase(it->second);
        guid_map_.erase(it++);
      } else {
        ++it;
      }
    }
  }
  db_.DeleteShortcutsWithUrl(shortcut_url);
  observer_list_->Notify(&ShortcutsBackendObserver::OnShortcutsRemoved,
                         shortcut_ids);
}

// NotificationObserver:
void ShortcutsBackend::Observe(int type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_HISTORY_URLS_DELETED) {
    const std::set<GURL>& urls =
        Details<const history::URLsDeletedDetails>(details)->urls;
    std::vector<std::string> shortcut_ids;

    base::AutoLock lock(data_access_lock_);
    for (shortcuts_provider::GuidToShortcutsIteratorMap::iterator
             it = guid_map_.begin();
         it != guid_map_.end(); ++it) {
      if (urls.find(it->second->second.url) != urls.end())
        shortcut_ids.push_back(it->first);
    }
    DeleteShortcutsWithIds(shortcut_ids);
    return;
  }

  DCHECK(type == chrome::NOTIFICATION_OMNIBOX_OPENED_URL);

  AutocompleteLog* log = Details<AutocompleteLog>(details).ptr();
  string16 text_lowercase(base::i18n::ToLower(log->text));

  int number_of_hits = 1;
  std::string id;
  const AutocompleteMatch& match(log->result.match_at(log->selected_index));
  base::AutoLock lock(data_access_lock_);
  for (shortcuts_provider::ShortcutMap::iterator it =
           shortcuts_map_.lower_bound(text_lowercase);
       it != shortcuts_map_.end() &&
           StartsWith(it->first, text_lowercase, true); ++it) {
    if (match.destination_url == it->second.url) {
      number_of_hits = it->second.number_of_hits + 1;
      id = it->second.id;
      // guid_map_ will be updated further down on re-insertion.
      shortcuts_map_.erase(it);
      break;
    }
  }
  shortcuts_provider::Shortcut shortcut(log->text, match.destination_url,
      match.contents, match.contents_class, match.description,
      match.description_class);
  shortcut.number_of_hits = number_of_hits;
  if (number_of_hits == 1)
    shortcut.id = guid::GenerateGUID();
  else
    shortcut.id = id;

  guid_map_[shortcut.id] = shortcuts_map_.insert(
      std::make_pair(text_lowercase, shortcut));

  if (number_of_hits == 1)
    AddShortcut(shortcut);
  else
    UpdateShortcut(shortcut);
}

}  // namespace history
