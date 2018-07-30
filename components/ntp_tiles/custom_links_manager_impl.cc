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

CustomLinksManagerImpl::CustomLinksManagerImpl(PrefService* prefs)
    : prefs_(prefs), store_(prefs), weak_ptr_factory_(this) {
  DCHECK(prefs);
  if (IsInitialized())
    current_links_ = store_.RetrieveLinks();
}

CustomLinksManagerImpl::~CustomLinksManagerImpl() = default;

bool CustomLinksManagerImpl::Initialize(const NTPTilesVector& tiles) {
  if (IsInitialized())
    return false;

  for (const NTPTile& tile : tiles)
    current_links_.emplace_back(Link{tile.url, tile.title});

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

  current_links_.emplace_back(Link{url, title});
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

  auto it = FindLinkWithUrl(url);
  if (it == current_links_.end())
    return false;

  if (!new_url.is_empty()) {
    // Do not update if |new_url| already exists in the list.
    if (!new_url.is_valid() ||
        FindLinkWithUrl(new_url) != current_links_.end()) {
      return false;
    }
    it->url = new_url;
  }

  if (!new_title.empty())
    it->title = new_title;

  store_.StoreLinks(current_links_);
  return true;
}

bool CustomLinksManagerImpl::DeleteLink(const GURL& url) {
  if (!IsInitialized() || !url.is_valid())
    return false;

  auto it = FindLinkWithUrl(url);
  if (it == current_links_.end())
    return false;

  prev_deleted_link_ =
      std::make_pair(std::distance(current_links_.begin(), it), *it);
  current_links_.erase(it);
  store_.StoreLinks(current_links_);
  return true;
}

bool CustomLinksManagerImpl::UndoDeleteLink() {
  if (!IsInitialized() || !prev_deleted_link_.has_value() ||
      current_links_.size() == kMaxNumLinks) {
    return false;
  }
  DCHECK_LE(prev_deleted_link_->first, current_links_.size());

  // Inserts the previously deleted link where it was located before.
  current_links_.insert(current_links_.begin() + prev_deleted_link_->first,
                        std::move(prev_deleted_link_->second));
  prev_deleted_link_ = base::nullopt;
  store_.StoreLinks(current_links_);
  return true;
}

void CustomLinksManagerImpl::ClearLinks() {
  store_.ClearLinks();
  current_links_.clear();
  prev_deleted_link_ = base::nullopt;
}

std::vector<CustomLinksManager::Link>::iterator
CustomLinksManagerImpl::FindLinkWithUrl(const GURL& url) {
  return std::find_if(current_links_.begin(), current_links_.end(),
                      [&url](const Link& link) { return link.url == url; });
}

// static
void CustomLinksManagerImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterBooleanPref(prefs::kCustomLinksInitialized, false);
  CustomLinksStore::RegisterProfilePrefs(user_prefs);
}

}  // namespace ntp_tiles
