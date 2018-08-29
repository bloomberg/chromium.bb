// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/custom_links_manager_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "components/ntp_tiles/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace ntp_tiles {

namespace {

const int kMaxNumLinks = 10;

}  // namespace

CustomLinksManagerImpl::CustomLinksManagerImpl(
    PrefService* prefs,
    history::HistoryService* history_service)
    : prefs_(prefs),
      store_(prefs),
      history_service_observer_(this),
      weak_ptr_factory_(this) {
  DCHECK(prefs);
  if (history_service)
    history_service_observer_.Add(history_service);
  if (IsInitialized())
    current_links_ = store_.RetrieveLinks();
}

CustomLinksManagerImpl::~CustomLinksManagerImpl() = default;

bool CustomLinksManagerImpl::Initialize(const NTPTilesVector& tiles) {
  if (IsInitialized())
    return false;

  for (const NTPTile& tile : tiles)
    current_links_.emplace_back(Link{tile.url, tile.title, true});

  store_.StoreLinks(current_links_);
  prefs_->SetBoolean(prefs::kCustomLinksInitialized, true);
  return true;
}

void CustomLinksManagerImpl::Uninitialize() {
  ClearLinks();
  prefs_->SetBoolean(prefs::kCustomLinksInitialized, false);
}

bool CustomLinksManagerImpl::IsInitialized() const {
  return prefs_->GetBoolean(prefs::kCustomLinksInitialized);
}

const std::vector<CustomLinksManager::Link>& CustomLinksManagerImpl::GetLinks()
    const {
  return current_links_;
}

bool CustomLinksManagerImpl::AddLink(const GURL& url,
                                     const base::string16& title) {
  if (!IsInitialized() || !url.is_valid() ||
      current_links_.size() == kMaxNumLinks) {
    return false;
  }

  if (FindLinkWithUrl(url) != current_links_.end())
    return false;

  previous_links_ = current_links_;
  current_links_.emplace_back(Link{url, title, false});
  store_.StoreLinks(current_links_);
  return true;
}

bool CustomLinksManagerImpl::UpdateLink(const GURL& url,
                                        const GURL& new_url,
                                        const base::string16& new_title) {
  if (!IsInitialized() || !url.is_valid() ||
      (new_url.is_empty() && new_title.empty())) {
    return false;
  }

  // Do not update if |new_url| is invalid or already exists in the list.
  if (!new_url.is_empty() &&
      (!new_url.is_valid() ||
       FindLinkWithUrl(new_url) != current_links_.end())) {
    return false;
  }

  auto it = FindLinkWithUrl(url);
  if (it == current_links_.end())
    return false;

  // At this point, we will be modifying at least one of the values.
  previous_links_ = current_links_;

  if (!new_url.is_empty())
    it->url = new_url;
  if (!new_title.empty())
    it->title = new_title;
  it->is_most_visited = false;

  store_.StoreLinks(current_links_);
  return true;
}

bool CustomLinksManagerImpl::DeleteLink(const GURL& url) {
  if (!IsInitialized() || !url.is_valid())
    return false;

  auto it = FindLinkWithUrl(url);
  if (it == current_links_.end())
    return false;

  previous_links_ = current_links_;
  current_links_.erase(it);
  store_.StoreLinks(current_links_);
  return true;
}

bool CustomLinksManagerImpl::UndoAction() {
  if (!IsInitialized() || !previous_links_.has_value())
    return false;

  // Replace the current links with the previous state.
  current_links_ = *previous_links_;
  previous_links_ = base::nullopt;
  store_.StoreLinks(current_links_);
  return true;
}

void CustomLinksManagerImpl::ClearLinks() {
  store_.ClearLinks();
  current_links_.clear();
  previous_links_ = base::nullopt;
}

std::vector<CustomLinksManager::Link>::iterator
CustomLinksManagerImpl::FindLinkWithUrl(const GURL& url) {
  return std::find_if(current_links_.begin(), current_links_.end(),
                      [&url](const Link& link) { return link.url == url; });
}

std::unique_ptr<base::CallbackList<void()>::Subscription>
CustomLinksManagerImpl::RegisterCallbackForOnChanged(
    base::RepeatingClosure callback) {
  return callback_list_.Add(callback);
}

// history::HistoryServiceObserver implementation.
void CustomLinksManagerImpl::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // We don't care about expired entries.
  if (!IsInitialized() || deletion_info.is_from_expiration())
    return;

  size_t initial_size = current_links_.size();
  if (deletion_info.IsAllHistory()) {
    base::EraseIf(current_links_,
                  [](auto& link) { return link.is_most_visited; });
  } else {
    for (const history::URLRow& row : deletion_info.deleted_rows()) {
      auto it = FindLinkWithUrl(row.url());
      if (it != current_links_.end() && it->is_most_visited)
        current_links_.erase(it);
    }
  }
  store_.StoreLinks(current_links_);
  previous_links_ = base::nullopt;

  // Alert MostVisitedSites that some links have been deleted.
  if (initial_size != current_links_.size())
    callback_list_.Notify();
}

void CustomLinksManagerImpl::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_service_observer_.RemoveAll();
}

// static
void CustomLinksManagerImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterBooleanPref(prefs::kCustomLinksInitialized, false);
  CustomLinksStore::RegisterProfilePrefs(user_prefs);
}

}  // namespace ntp_tiles
