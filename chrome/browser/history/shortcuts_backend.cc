// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/shortcuts_backend.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete_log.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/shortcuts_database.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace {

// Takes Match classification vector and removes all matched positions,
// compacting repetitions if necessary.
void StripMatchMarkersFromClassifications(ACMatchClassifications* matches) {
  DCHECK(matches);
  ACMatchClassifications unmatched;
  for (ACMatchClassifications::iterator i = matches->begin();
       i != matches->end(); ++i) {
    AutocompleteMatch::AddLastClassificationIfNecessary(&unmatched, i->offset,
        i->style & ~ACMatchClassification::MATCH);
  }
  matches->swap(unmatched);
}

}  // namespace

namespace history {

// ShortcutsBackend::Shortcut -------------------------------------------------

ShortcutsBackend::Shortcut::Shortcut(
    const std::string& id,
    const string16& text,
    const GURL& url,
    const string16& contents,
    const ACMatchClassifications& contents_class,
    const string16& description,
    const ACMatchClassifications& description_class,
    const base::Time& last_access_time,
    int number_of_hits)
    : id(id),
      text(text),
      url(url),
      contents(contents),
      contents_class(contents_class),
      description(description),
      description_class(description_class),
      last_access_time(last_access_time),
      number_of_hits(number_of_hits) {
  StripMatchMarkersFromClassifications(&this->contents_class);
  StripMatchMarkersFromClassifications(&this->description_class);
}

ShortcutsBackend::Shortcut::Shortcut()
    : last_access_time(base::Time::Now()),
      number_of_hits(0) {
}

ShortcutsBackend::Shortcut::~Shortcut() {
}


// ShortcutsBackend -----------------------------------------------------------

ShortcutsBackend::ShortcutsBackend(Profile* profile, bool suppress_db)
    : current_state_(NOT_INITIALIZED),
      no_db_access_(suppress_db) {
  if (!suppress_db)
    db_ = new ShortcutsDatabase(profile);
  // |profile| can be NULL in tests.
  if (profile) {
    notification_registrar_.Add(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                                content::Source<Profile>(profile));
    notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                                content::Source<Profile>(profile));
  }
}

bool ShortcutsBackend::Init() {
  if (current_state_ != NOT_INITIALIZED)
    return false;

  if (no_db_access_) {
    current_state_ = INITIALIZED;
    return true;
  }

  current_state_ = INITIALIZING;
  return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&ShortcutsBackend::InitInternal, this));
}

bool ShortcutsBackend::AddShortcut(const Shortcut& shortcut) {
  if (!initialized())
    return false;
  DCHECK(guid_map_.find(shortcut.id) == guid_map_.end());
  guid_map_[shortcut.id] = shortcuts_map_.insert(
      std::make_pair(base::i18n::ToLower(shortcut.text), shortcut));
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  return no_db_access_ || BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&ShortcutsDatabase::AddShortcut),
                 db_.get(), shortcut));
}

bool ShortcutsBackend::UpdateShortcut(const Shortcut& shortcut) {
  if (!initialized())
    return false;
  GuidToShortcutsIteratorMap::iterator it = guid_map_.find(shortcut.id);
  if (it != guid_map_.end())
    shortcuts_map_.erase(it->second);
  guid_map_[shortcut.id] = shortcuts_map_.insert(
      std::make_pair(base::i18n::ToLower(shortcut.text), shortcut));
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  return no_db_access_ || BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&ShortcutsDatabase::UpdateShortcut),
                 db_.get(), shortcut));
}

bool ShortcutsBackend::DeleteShortcutsWithIds(
    const std::vector<std::string>& shortcut_ids) {
  if (!initialized())
    return false;
  for (size_t i = 0; i < shortcut_ids.size(); ++i) {
    GuidToShortcutsIteratorMap::iterator it = guid_map_.find(shortcut_ids[i]);
    if (it != guid_map_.end()) {
      shortcuts_map_.erase(it->second);
      guid_map_.erase(it);
    }
  }
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  return no_db_access_ || BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&ShortcutsDatabase::DeleteShortcutsWithIds),
                 db_.get(), shortcut_ids));
}

bool ShortcutsBackend::DeleteShortcutsWithUrl(const GURL& shortcut_url) {
  if (!initialized())
    return false;
  std::vector<std::string> shortcut_ids;
  for (GuidToShortcutsIteratorMap::iterator it = guid_map_.begin();
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
  return no_db_access_ || BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&ShortcutsDatabase::DeleteShortcutsWithUrl),
                 db_.get(), shortcut_url.spec()));
}

bool ShortcutsBackend::DeleteAllShortcuts() {
  if (!initialized())
    return false;
  shortcuts_map_.clear();
  guid_map_.clear();
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  return no_db_access_ || BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&ShortcutsDatabase::DeleteAllShortcuts),
                 db_.get()));
}

ShortcutsBackend::~ShortcutsBackend() {}

void ShortcutsBackend::InitInternal() {
  DCHECK(current_state_ == INITIALIZING);
  db_->Init();
  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  db_->LoadShortcuts(&shortcuts);
  temp_shortcuts_map_.reset(new ShortcutMap);
  temp_guid_map_.reset(new GuidToShortcutsIteratorMap);
  for (ShortcutsDatabase::GuidToShortcutMap::iterator it = shortcuts.begin();
       it != shortcuts.end(); ++it) {
    (*temp_guid_map_)[it->first] = temp_shortcuts_map_->insert(
        std::make_pair(base::i18n::ToLower(it->second.text), it->second));
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
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
    const URLRows& rows(
        content::Details<const history::URLsDeletedDetails>(details)->rows);
    std::vector<std::string> shortcut_ids;

    for (GuidToShortcutsIteratorMap::iterator it = guid_map_.begin();
         it != guid_map_.end(); ++it) {
      if (std::find_if(rows.begin(), rows.end(),
                       URLRow::URLRowHasURL(it->second->second.url)) !=
          rows.end())
        shortcut_ids.push_back(it->first);
    }
    DeleteShortcutsWithIds(shortcut_ids);
    return;
  }

  DCHECK(type == chrome::NOTIFICATION_OMNIBOX_OPENED_URL);

  AutocompleteLog* log = content::Details<AutocompleteLog>(details).ptr();
  string16 text_lowercase(base::i18n::ToLower(log->text));

  const AutocompleteMatch& match(log->result.match_at(log->selected_index));
  for (ShortcutMap::iterator it = shortcuts_map_.lower_bound(text_lowercase);
       it != shortcuts_map_.end() &&
           StartsWith(it->first, text_lowercase, true); ++it) {
    if (match.destination_url == it->second.url) {
      UpdateShortcut(Shortcut(it->second.id, log->text, match.destination_url,
          match.contents, match.contents_class, match.description,
          match.description_class, base::Time::Now(),
          it->second.number_of_hits + 1));
      return;
    }
  }
  AddShortcut(Shortcut(base::GenerateGUID(), log->text, match.destination_url,
      match.contents, match.contents_class, match.description,
      match.description_class, base::Time::Now(), 1));
}

void ShortcutsBackend::ShutdownOnUIThread() {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  notification_registrar_.RemoveAll();
}

}  // namespace history
