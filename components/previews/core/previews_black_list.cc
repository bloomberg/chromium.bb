// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_black_list.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/previews/core/previews_black_list_item.h"
#include "components/previews/core/previews_experiments.h"
#include "url/gurl.h"

namespace previews {

PreviewsBlackList::PreviewsBlackList(
    std::unique_ptr<PreviewsOptOutStore> opt_out_store,
    std::unique_ptr<base::Clock> clock)
    : loaded_(false),
      opt_out_store_(std::move(opt_out_store)),
      clock_(std::move(clock)),
      weak_factory_(this) {
  if (opt_out_store_) {
    opt_out_store_->LoadBlackList(base::Bind(
        &PreviewsBlackList::LoadBlackListDone, weak_factory_.GetWeakPtr()));
  } else {
    LoadBlackListDone(base::MakeUnique<BlackListItemMap>());
  }
}

PreviewsBlackList::~PreviewsBlackList() {}

void PreviewsBlackList::AddPreviewNavigation(const GURL& url,
                                             bool opt_out,
                                             PreviewsType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(url.has_host());
  // If the |black_list_item_map_| has been loaded from |opt_out_store_|,
  // synchronous operations will be accurate. Otherwise, queue the task to run
  // asynchronously.
  if (loaded_) {
    AddPreviewNavigationSync(url, opt_out, type);
  } else {
    QueuePendingTask(base::Bind(&PreviewsBlackList::AddPreviewNavigationSync,
                                base::Unretained(this), url, opt_out, type));
  }
}

void PreviewsBlackList::AddPreviewNavigationSync(const GURL& url,
                                                 bool opt_out,
                                                 PreviewsType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(url.has_host());
  DCHECK(loaded_);
  std::string host_name = url.host();
  base::Time now = clock_->Now();
  PreviewsBlackListItem* item = GetBlackListItem(host_name);
  if (!item) {
    item = CreateBlackListItem(host_name);
  }
  item->AddPreviewNavigation(opt_out, now);
  DCHECK_LE(black_list_item_map_->size(),
            params::MaxInMemoryHostsInBlackList());
  if (!opt_out_store_)
    return;
  opt_out_store_->AddPreviewNavigation(opt_out, host_name, type, now);
}

bool PreviewsBlackList::IsLoadedAndAllowed(const GURL& url,
                                           PreviewsType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(url.has_host());
  if (!loaded_)
    return false;
  PreviewsBlackListItem* black_list_item = GetBlackListItem(url.host());
  return !black_list_item || !black_list_item->IsBlackListed(clock_->Now());
}

void PreviewsBlackList::ClearBlackList(base::Time begin_time,
                                       base::Time end_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(begin_time, end_time);
  // If the |black_list_item_map_| has been loaded from |opt_out_store_|,
  // synchronous operations will be accurate. Otherwise, queue the task to run
  // asynchronously.
  if (loaded_) {
    ClearBlackListSync(begin_time, end_time);
  } else {
    QueuePendingTask(base::Bind(&PreviewsBlackList::ClearBlackListSync,
                                base::Unretained(this), begin_time, end_time));
  }
}

void PreviewsBlackList::ClearBlackListSync(base::Time begin_time,
                                           base::Time end_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(loaded_);
  DCHECK_LE(begin_time, end_time);
  black_list_item_map_.reset(nullptr);
  loaded_ = false;
  // Delete relevant entries and reload the blacklist into memory.
  if (opt_out_store_) {
    opt_out_store_->ClearBlackList(begin_time, end_time);
    opt_out_store_->LoadBlackList(base::Bind(
        &PreviewsBlackList::LoadBlackListDone, weak_factory_.GetWeakPtr()));
  } else {
    LoadBlackListDone(base::MakeUnique<BlackListItemMap>());
  }
}

void PreviewsBlackList::QueuePendingTask(base::Closure callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!loaded_);
  DCHECK(!callback.is_null());
  pending_callbacks_.emplace(callback);
}

void PreviewsBlackList::LoadBlackListDone(
    std::unique_ptr<BlackListItemMap> black_list_item_map) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(black_list_item_map->size(), params::MaxInMemoryHostsInBlackList());
  loaded_ = true;
  black_list_item_map_ = std::move(black_list_item_map);

  // Run all pending tasks. |loaded_| may change if ClearBlackList is queued.
  while (pending_callbacks_.size() > 0 && loaded_) {
    pending_callbacks_.front().Run();
    pending_callbacks_.pop();
  }
}

PreviewsBlackListItem* PreviewsBlackList::GetBlackListItem(
    const std::string& host_name) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(loaded_);
  BlackListItemMap::iterator iter = black_list_item_map_->find(host_name);
  if (iter != black_list_item_map_->end())
    return iter->second.get();
  return nullptr;
}

PreviewsBlackListItem* PreviewsBlackList::CreateBlackListItem(
    const std::string& host_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(loaded_);
  DCHECK(!GetBlackListItem(host_name));
  if (black_list_item_map_->size() >= params::MaxInMemoryHostsInBlackList())
    EvictOldestOptOut();
  DCHECK_LT(black_list_item_map_->size(),
            params::MaxInMemoryHostsInBlackList());
  PreviewsBlackListItem* black_list_item = new PreviewsBlackListItem(
      params::MaxStoredHistoryLengthForBlackList(),
      params::BlackListOptOutThreshold(), params::BlackListDuration());
  black_list_item_map_->operator[](host_name) =
      base::WrapUnique(black_list_item);
  return black_list_item;
}

void PreviewsBlackList::EvictOldestOptOut() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(loaded_);
  // TODO(ryansturm): Add UMA. crbug.com/647717
  BlackListItemMap::iterator item_to_delete = black_list_item_map_->end();
  base::Time oldest_opt_out = clock_->Now();
  for (BlackListItemMap::iterator iter = black_list_item_map_->begin();
       iter != black_list_item_map_->end(); ++iter) {
    if (!iter->second->most_recent_opt_out_time()) {
      // If there is no opt out time, this is a good choice to evict.
      item_to_delete = iter;
      break;
    }
    if (iter->second->most_recent_opt_out_time().value() < oldest_opt_out) {
      oldest_opt_out = iter->second->most_recent_opt_out_time().value();
      item_to_delete = iter;
    }
  }
  black_list_item_map_->erase(item_to_delete);
}

}  // namespace previews
