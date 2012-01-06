// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/shortcuts_backend.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/case_conversion.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/shortcuts_database.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/guid.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace history {

ShortcutsBackend::ShortcutsBackend(const FilePath& db_folder_path,
                                   Profile *profile)
    : current_state_(NOT_INITIALIZED),
      db_(new ShortcutsDatabase(db_folder_path)),
      no_db_access_(db_folder_path.empty()) {
  // |profile| can be NULL in tests.
  if (profile) {
    notification_registrar_.Add(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                                content::Source<Profile>(profile));
    notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                                content::Source<Profile>(profile));
  }
}

ShortcutsBackend::~ShortcutsBackend() {}

bool ShortcutsBackend::Init() {
  if (current_state_ == NOT_INITIALIZED) {
    current_state_ = INITIALIZING;
    if (no_db_access_) {
      current_state_ = INITIALIZED;
      return true;
    } else {
      return BrowserThread::PostTask(
          BrowserThread::DB, FROM_HERE,
          base::Bind(&ShortcutsBackend::InitInternal, this));
    }
  } else {
    return false;
  }
}

bool ShortcutsBackend::AddShortcut(
    const shortcuts_provider::Shortcut& shortcut) {
  if (!initialized())
    return false;
  DCHECK(guid_map_.find(shortcut.id) == guid_map_.end());
  guid_map_[shortcut.id] = shortcuts_map_.insert(
      std::make_pair(base::i18n::ToLower(shortcut.text), shortcut));
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  if (no_db_access_)
    return true;
  return BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&ShortcutsDatabase::AddShortcut),
                 db_.get(), shortcut));
}

bool ShortcutsBackend::UpdateShortcut(
    const shortcuts_provider::Shortcut& shortcut) {
  if (!initialized())
    return false;
  shortcuts_provider::GuidToShortcutsIteratorMap::iterator it =
      guid_map_.find(shortcut.id);
  if (it != guid_map_.end())
    shortcuts_map_.erase(it->second);
  guid_map_[shortcut.id] = shortcuts_map_.insert(
      std::make_pair(base::i18n::ToLower(shortcut.text), shortcut));
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  if (no_db_access_)
    return true;
  return BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
          base::Bind(base::IgnoreResult(&ShortcutsDatabase::UpdateShortcut),
                     db_.get(), shortcut));
}

bool ShortcutsBackend::DeleteShortcutsWithIds(
    const std::vector<std::string>& shortcut_ids) {
  if (!initialized())
    return false;
  for (size_t i = 0; i < shortcut_ids.size(); ++i) {
    shortcuts_provider::GuidToShortcutsIteratorMap::iterator it =
        guid_map_.find(shortcut_ids[i]);
    if (it != guid_map_.end()) {
      shortcuts_map_.erase(it->second);
      guid_map_.erase(it);
    }
  }
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  if (no_db_access_)
    return true;
  return BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(
          base::IgnoreResult(&ShortcutsDatabase::DeleteShortcutsWithIds),
          db_.get(), shortcut_ids));
}

bool ShortcutsBackend::DeleteShortcutsWithUrl(const GURL& shortcut_url) {
  if (!initialized())
    return false;
  std::vector<std::string> shortcut_ids;
  for (shortcuts_provider::GuidToShortcutsIteratorMap::iterator
           it = guid_map_.begin();
       it != guid_map_.end();) {
    if (it->second->second.url == shortcut_url) {
      shortcut_ids.push_back(it->first);
      shortcuts_map_.erase(it->second);
      guid_map_.erase(it++);
    } else {
      ++it;
    }
  }
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  if (no_db_access_)
    return true;
  return BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(
          base::IgnoreResult(&ShortcutsDatabase::DeleteShortcutsWithUrl),
          db_.get(), shortcut_url.spec()));
}

bool ShortcutsBackend::DeleteAllShortcuts() {
  if (!initialized())
    return false;
  shortcuts_map_.clear();
  guid_map_.clear();
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  if (no_db_access_)
    return true;
  return BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(
          base::IgnoreResult(&ShortcutsDatabase::DeleteAllShortcuts),
          db_.get()));
}

void ShortcutsBackend::InitInternal() {
  DCHECK(current_state_ == INITIALIZING);
  db_->Init();
  shortcuts_provider::GuidToShortcutMap shortcuts;
  db_->LoadShortcuts(&shortcuts);
  temp_shortcuts_map_.reset(new shortcuts_provider::ShortcutMap);
  temp_guid_map_.reset(new shortcuts_provider::GuidToShortcutsIteratorMap);
  for (shortcuts_provider::GuidToShortcutMap::iterator it = shortcuts.begin();
       it != shortcuts.end(); ++it) {
    (*temp_guid_map_)[it->first] = temp_shortcuts_map_->insert(
        std::make_pair(base::i18n::ToLower(it->second.text), it->second));
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ShortcutsBackend::InitCompleted, this));
}

void ShortcutsBackend::InitCompleted() {
  temp_guid_map_->swap(guid_map_);
  temp_shortcuts_map_->swap(shortcuts_map_);
  temp_shortcuts_map_.reset(NULL);
  temp_guid_map_.reset(NULL);
  current_state_ = INITIALIZED;
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsLoaded());
}

// content::NotificationObserver:
void ShortcutsBackend::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  if (current_state_ != INITIALIZED)
    return;
  if (type == chrome::NOTIFICATION_HISTORY_URLS_DELETED) {
    if (content::Details<const history::URLsDeletedDetails>(details)->
            all_history) {
      DeleteAllShortcuts();
    }
    const std::set<GURL>& urls =
        content::Details<const history::URLsDeletedDetails>(details)->urls;
    std::vector<std::string> shortcut_ids;

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

  AutocompleteLog* log = content::Details<AutocompleteLog>(details).ptr();
  string16 text_lowercase(base::i18n::ToLower(log->text));

  int number_of_hits = 1;
  std::string id;
  const AutocompleteMatch& match(log->result.match_at(log->selected_index));
  for (shortcuts_provider::ShortcutMap::iterator it =
           shortcuts_map_.lower_bound(text_lowercase);
       it != shortcuts_map_.end() &&
           StartsWith(it->first, text_lowercase, true); ++it) {
    if (match.destination_url == it->second.url) {
      number_of_hits = it->second.number_of_hits + 1;
      id = it->second.id;
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

  if (number_of_hits == 1)
    AddShortcut(shortcut);
  else
    UpdateShortcut(shortcut);
}

}  // namespace history
