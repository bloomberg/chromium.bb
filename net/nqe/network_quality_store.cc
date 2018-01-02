// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_quality_store.h"

#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/network_change_notifier.h"

namespace net {

namespace nqe {

namespace internal {

NetworkQualityStore::NetworkQualityStore() : weak_ptr_factory_(this) {
  static_assert(kMaximumNetworkQualityCacheSize > 0,
                "Size of the network quality cache must be > 0");
  // This limit should not be increased unless the logic for removing the
  // oldest cache entry is rewritten to use a doubly-linked-list LRU queue.
  static_assert(kMaximumNetworkQualityCacheSize <= 20,
                "Size of the network quality cache must <= 20");
}

NetworkQualityStore::~NetworkQualityStore() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void NetworkQualityStore::Add(
    const nqe::internal::NetworkID& network_id,
    const nqe::internal::CachedNetworkQuality& cached_network_quality) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(cached_network_qualities_.size(),
            static_cast<size_t>(kMaximumNetworkQualityCacheSize));

  if (cached_network_quality.effective_connection_type() ==
      EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    return;
  }

  // Remove the entry from the map, if it is already present.
  cached_network_qualities_.erase(network_id);

  if (cached_network_qualities_.size() == kMaximumNetworkQualityCacheSize) {
    // Remove the oldest entry.
    CachedNetworkQualities::iterator oldest_entry_iterator =
        cached_network_qualities_.begin();

    for (CachedNetworkQualities::iterator it =
             cached_network_qualities_.begin();
         it != cached_network_qualities_.end(); ++it) {
      if ((it->second).OlderThan(oldest_entry_iterator->second))
        oldest_entry_iterator = it;
    }
    cached_network_qualities_.erase(oldest_entry_iterator);
  }

  cached_network_qualities_.insert(
      std::make_pair(network_id, cached_network_quality));
  DCHECK_LE(cached_network_qualities_.size(),
            static_cast<size_t>(kMaximumNetworkQualityCacheSize));

  for (auto& observer : network_qualities_cache_observer_list_)
    observer.OnChangeInCachedNetworkQuality(network_id, cached_network_quality);
}

bool NetworkQualityStore::GetById(
    const nqe::internal::NetworkID& network_id,
    nqe::internal::CachedNetworkQuality* cached_network_quality) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  // |matching_it| points to the entry that has the same connection type and
  // id as |network_id|, and has the signal strength closest to the signal
  // stength of |network_id|.
  CachedNetworkQualities::const_iterator matching_it =
      cached_network_qualities_.end();
  int matching_it_diff_signal_strength = INT32_MAX;

  for (CachedNetworkQualities::const_iterator it =
           cached_network_qualities_.begin();
       it != cached_network_qualities_.end(); ++it) {
    if (network_id.type != it->first.type || network_id.id != it->first.id) {
      // The |type| and |id| must match.
      continue;
    }

    if (network_id.signal_strength == INT32_MIN) {
      // Current network does not have a valid signal strength value. Return the
      // entry without searching for the entry with the closest signal strength.
      matching_it = it;
      break;
    }

    // Determine if the signal strength of |network_id| is closer to the
    // signal strength of the network at |it| then that of the network at
    // |matching_it|.
    int diff_signal_strength =
        std::abs(network_id.signal_strength - it->first.signal_strength);
    if (it->first.signal_strength == INT32_MIN)
      diff_signal_strength = INT32_MAX;

    if (matching_it == cached_network_qualities_.end() ||
        diff_signal_strength < matching_it_diff_signal_strength) {
      matching_it = it;
      matching_it_diff_signal_strength = diff_signal_strength;
    }
  }

  if (matching_it == cached_network_qualities_.end())
    return false;

  *cached_network_quality = matching_it->second;
  return true;
}

void NetworkQualityStore::AddNetworkQualitiesCacheObserver(
    NetworkQualitiesCacheObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_qualities_cache_observer_list_.AddObserver(observer);

  // Notify the |observer| on the next message pump since |observer| may not
  // be completely set up for receiving the callbacks.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&NetworkQualityStore::NotifyCacheObserverIfPresent,
                            weak_ptr_factory_.GetWeakPtr(), observer));
}

void NetworkQualityStore::RemoveNetworkQualitiesCacheObserver(
    NetworkQualitiesCacheObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_qualities_cache_observer_list_.RemoveObserver(observer);
}

void NetworkQualityStore::NotifyCacheObserverIfPresent(
    NetworkQualitiesCacheObserver* observer) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!network_qualities_cache_observer_list_.HasObserver(observer))
    return;
  for (const auto it : cached_network_qualities_)
    observer->OnChangeInCachedNetworkQuality(it.first, it.second);
}

}  // namespace internal

}  // namespace nqe

}  // namespace net
