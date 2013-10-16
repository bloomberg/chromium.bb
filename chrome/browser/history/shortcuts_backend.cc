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
#include "base/strings/string_util.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/shortcuts_database.h"
#include "chrome/browser/omnibox/omnibox_log.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace {

// Takes Match classification vector and removes all matched positions,
// compacting repetitions if necessary.
ACMatchClassifications StripMatchMarkers(
    const ACMatchClassifications& matches) {
  ACMatchClassifications unmatched;
  for (ACMatchClassifications::const_iterator i(matches.begin());
       i != matches.end(); ++i) {
    AutocompleteMatch::AddLastClassificationIfNecessary(
        &unmatched, i->offset, i->style & ~ACMatchClassification::MATCH);
  }
  return unmatched;
}

// Normally shortcuts have the same match type as the original match they were
// created from, but for certain match types, we should modify the shortcut's
// type slightly to reflect that the origin of the shortcut is historical.
AutocompleteMatch::Type GetTypeForShortcut(AutocompleteMatch::Type type) {
  switch (type) {
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
    case AutocompleteMatchType::NAVSUGGEST:
      return AutocompleteMatchType::HISTORY_URL;

    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatchType::SEARCH_SUGGEST:
      return AutocompleteMatchType::SEARCH_HISTORY;

    default:
      return type;
  }
}

}  // namespace

namespace history {

// ShortcutsBackend::Shortcut::MatchCore --------------------------------------

ShortcutsBackend::Shortcut::MatchCore::MatchCore(
    const AutocompleteMatch& match)
    : fill_into_edit(match.fill_into_edit),
      destination_url(match.destination_url),
      contents(match.contents),
      contents_class(StripMatchMarkers(match.contents_class)),
      description(match.description),
      description_class(StripMatchMarkers(match.description_class)),
      transition(match.transition),
      type(GetTypeForShortcut(match.type)),
      keyword(match.keyword) {
}

ShortcutsBackend::Shortcut::MatchCore::MatchCore(
    const string16& fill_into_edit,
    const GURL& destination_url,
    const string16& contents,
    const ACMatchClassifications& contents_class,
    const string16& description,
    const ACMatchClassifications& description_class,
    content::PageTransition transition,
    AutocompleteMatch::Type type,
    const string16& keyword)
    : fill_into_edit(fill_into_edit),
      destination_url(destination_url),
      contents(contents),
      contents_class(StripMatchMarkers(contents_class)),
      description(description),
      description_class(StripMatchMarkers(description_class)),
      transition(transition),
      type(GetTypeForShortcut(type)),
      keyword(keyword) {
}

ShortcutsBackend::Shortcut::MatchCore::~MatchCore() {
}

AutocompleteMatch ShortcutsBackend::Shortcut::MatchCore::ToMatch() const {
  AutocompleteMatch match;
  match.fill_into_edit = fill_into_edit;
  match.destination_url = destination_url;
  match.contents = contents;
  match.contents_class = contents_class;
  match.description = description;
  match.description_class = description_class;
  match.transition = transition;
  match.type = type;
  match.keyword = keyword;
  return match;
}


// ShortcutsBackend::Shortcut -------------------------------------------------

ShortcutsBackend::Shortcut::Shortcut(
    const std::string& id,
    const string16& text,
    const MatchCore& match_core,
    const base::Time& last_access_time,
    int number_of_hits)
    : id(id),
      text(text),
      match_core(match_core),
      last_access_time(last_access_time),
      number_of_hits(number_of_hits) {
}

ShortcutsBackend::Shortcut::Shortcut()
    : match_core(AutocompleteMatch()),
      last_access_time(base::Time::Now()),
      number_of_hits(0) {
}

ShortcutsBackend::Shortcut::~Shortcut() {
}


// ShortcutsBackend -----------------------------------------------------------

ShortcutsBackend::ShortcutsBackend(Profile* profile, bool suppress_db)
    : current_state_(NOT_INITIALIZED),
      no_db_access_(suppress_db) {
  if (!suppress_db) {
    db_ = new ShortcutsDatabase(
        profile->GetPath().Append(chrome::kShortcutsDatabaseName));
  }
  // |profile| can be NULL in tests.
  if (profile) {
    notification_registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
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

bool ShortcutsBackend::DeleteShortcutsWithUrl(const GURL& shortcut_url) {
  return initialized() && DeleteShortcutsWithUrl(shortcut_url, true);
}

void ShortcutsBackend::AddObserver(ShortcutsBackendObserver* obs) {
  observer_list_.AddObserver(obs);
}

void ShortcutsBackend::RemoveObserver(ShortcutsBackendObserver* obs) {
  observer_list_.RemoveObserver(obs);
}

void ShortcutsBackend::OnOmniboxNavigation(const string16& text,
                                           const AutocompleteMatch& match) {
  const string16 text_lowercase(base::i18n::ToLower(text));
  const base::Time now(base::Time::Now());
  for (ShortcutMap::const_iterator it(
       shortcuts_map_.lower_bound(text_lowercase));
       it != shortcuts_map_.end() &&
           StartsWith(it->first, text_lowercase, true); ++it) {
    if (match.destination_url == it->second.match_core.destination_url) {
      UpdateShortcut(Shortcut(it->second.id, text, Shortcut::MatchCore(match),
                              now, it->second.number_of_hits + 1));
      return;
    }
  }
  AddShortcut(Shortcut(base::GenerateGUID(), text, Shortcut::MatchCore(match),
                       now, 1));
}

ShortcutsBackend::~ShortcutsBackend() {
}

void ShortcutsBackend::ShutdownOnUIThread() {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  notification_registrar_.RemoveAll();
}

void ShortcutsBackend::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  if (!initialized())
    return;

  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    // When an extension is unloaded, we want to remove any Shortcuts associated
    // with it.
    DeleteShortcutsWithUrl(content::Details<extensions::UnloadedExtensionInfo>(
        details)->extension->url(), false);
    return;
  }

  DCHECK_EQ(chrome::NOTIFICATION_HISTORY_URLS_DELETED, type);
  const history::URLsDeletedDetails* deleted_details =
      content::Details<const history::URLsDeletedDetails>(details).ptr();
  if (deleted_details->all_history)
    DeleteAllShortcuts();
  const URLRows& rows(deleted_details->rows);
  std::vector<std::string> shortcut_ids;

  for (GuidMap::const_iterator it(guid_map_.begin()); it != guid_map_.end();
        ++it) {
    if (std::find_if(
        rows.begin(), rows.end(), URLRow::URLRowHasURL(
            it->second->second.match_core.destination_url)) != rows.end())
      shortcut_ids.push_back(it->first);
  }
  DeleteShortcutsWithIds(shortcut_ids);
}

void ShortcutsBackend::InitInternal() {
  DCHECK(current_state_ == INITIALIZING);
  db_->Init();
  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  db_->LoadShortcuts(&shortcuts);
  temp_shortcuts_map_.reset(new ShortcutMap);
  temp_guid_map_.reset(new GuidMap);
  for (ShortcutsDatabase::GuidToShortcutMap::const_iterator it(
       shortcuts.begin()); it != shortcuts.end(); ++it) {
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
  GuidMap::iterator it(guid_map_.find(shortcut.id));
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
    GuidMap::iterator it(guid_map_.find(shortcut_ids[i]));
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

bool ShortcutsBackend::DeleteShortcutsWithUrl(const GURL& url,
                                              bool exact_match) {
  const std::string& url_spec = url.spec();
  std::vector<std::string> shortcut_ids;
  for (GuidMap::iterator it(guid_map_.begin()); it != guid_map_.end(); ) {
    if (exact_match ?
        (it->second->second.match_core.destination_url == url) :
        StartsWithASCII(it->second->second.match_core.destination_url.spec(),
                        url_spec, true)) {
      shortcut_ids.push_back(it->first);
      shortcuts_map_.erase(it->second);
      guid_map_.erase(it++);
    } else {
      ++it;
    }
  }
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  return no_db_access_ ||
      BrowserThread::PostTask(
          BrowserThread::DB, FROM_HERE,
          base::Bind(
              base::IgnoreResult(&ShortcutsDatabase::DeleteShortcutsWithUrl),
              db_.get(), url_spec));
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

}  // namespace history
