// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/sdch/sdch_owner.h"

#include "base/bind.h"
#include "base/debug/alias.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "net/base/sdch_manager.h"
#include "net/base/sdch_net_log_params.h"

namespace {

enum DictionaryFate {
  // A Get-Dictionary header wasn't acted on.
  DICTIONARY_FATE_GET_IGNORED = 1,

  // A fetch was attempted, but failed.
  // TODO(rdsmith): Actually record this case.
  DICTIONARY_FATE_FETCH_FAILED = 2,

  // A successful fetch was dropped on the floor, no space.
  DICTIONARY_FATE_FETCH_IGNORED_NO_SPACE = 3,

  // A successful fetch was refused by the SdchManager.
  DICTIONARY_FATE_FETCH_MANAGER_REFUSED = 4,

  // A dictionary was successfully added.
  DICTIONARY_FATE_ADDED = 5,

  // A dictionary was evicted by an incoming dict.
  DICTIONARY_FATE_EVICT_FOR_DICT = 6,

  // A dictionary was evicted by memory pressure.
  DICTIONARY_FATE_EVICT_FOR_MEMORY = 7,

  // A dictionary was evicted on destruction.
  DICTIONARY_FATE_EVICT_FOR_DESTRUCTION = 8,

  DICTIONARY_FATE_MAX = 9
};

void RecordDictionaryFate(enum DictionaryFate fate) {
  UMA_HISTOGRAM_ENUMERATION("Sdch3.DictionaryFate", fate, DICTIONARY_FATE_MAX);
}

void RecordDictionaryEviction(int use_count, DictionaryFate fate) {
  DCHECK(fate == DICTIONARY_FATE_EVICT_FOR_DICT ||
         fate == DICTIONARY_FATE_EVICT_FOR_MEMORY ||
         fate == DICTIONARY_FATE_EVICT_FOR_DESTRUCTION);

  UMA_HISTOGRAM_COUNTS_100("Sdch3.DictionaryUseCount", use_count);
  RecordDictionaryFate(fate);
}

}  // namespace

namespace net {

// Adjust SDCH limits downwards for mobile.
#if defined(OS_ANDROID) || defined(OS_IOS)
// static
const size_t SdchOwner::kMaxTotalDictionarySize = 500 * 1000;
#else
// static
const size_t SdchOwner::kMaxTotalDictionarySize = 20 * 1000 * 1000;
#endif

// Somewhat arbitrary, but we assume a dictionary smaller than
// 50K isn't going to do anyone any good.  Note that this still doesn't
// prevent download and addition unless there is less than this
// amount of space available in storage.
const size_t SdchOwner::kMinSpaceForDictionaryFetch = 50 * 1000;

SdchOwner::SdchOwner(net::SdchManager* sdch_manager,
                     net::URLRequestContext* context)
    : manager_(sdch_manager),
      fetcher_(context,
               base::Bind(&SdchOwner::OnDictionaryFetched,
                          // Because |fetcher_| is owned by SdchOwner, the
                          // SdchOwner object will be available for the lifetime
                          // of |fetcher_|.
                          base::Unretained(this))),
      total_dictionary_bytes_(0),
      clock_(new base::DefaultClock),
      max_total_dictionary_size_(kMaxTotalDictionarySize),
      min_space_for_dictionary_fetch_(kMinSpaceForDictionaryFetch)
      // TODO(rmcilroy) Add back memory_pressure_listener_ when
      // http://crbug.com/447208 is fixed
#if defined(OS_CHROMEOS)
      // For debugging http://crbug.com/454198; remove when resolved.
      , destroyed_(0)
#endif
      {
#if defined(OS_CHROMEOS)
  // For debugging http://crbug.com/454198; remove when resolved.
  CHECK(clock_.get());
#endif
  manager_->AddObserver(this);
}

SdchOwner::~SdchOwner() {
#if defined(OS_CHROMEOS)
  // For debugging http://crbug.com/454198; remove when resolved.
  CHECK_EQ(0u, destroyed_);
  CHECK(clock_.get());
  clock_.reset();
#endif

  for (auto it = local_dictionary_info_.begin();
       it != local_dictionary_info_.end(); ++it) {
    RecordDictionaryEviction(it->second.use_count,
                             DICTIONARY_FATE_EVICT_FOR_DESTRUCTION);
  }
  manager_->RemoveObserver(this);
#if defined(OS_CHROMEOS)
  destroyed_ = 0xdeadbeef;
#endif
}

void SdchOwner::SetMaxTotalDictionarySize(size_t max_total_dictionary_size) {
  max_total_dictionary_size_ = max_total_dictionary_size;
}

void SdchOwner::SetMinSpaceForDictionaryFetch(
    size_t min_space_for_dictionary_fetch) {
  min_space_for_dictionary_fetch_ = min_space_for_dictionary_fetch;
}

void SdchOwner::OnDictionaryFetched(const std::string& dictionary_text,
                                    const GURL& dictionary_url,
                                    const net::BoundNetLog& net_log) {
  struct DictionaryItem {
    base::Time last_used;
    std::string server_hash;
    int use_count;
    size_t dictionary_size;

    DictionaryItem() : use_count(0), dictionary_size(0) {}
    DictionaryItem(const base::Time& last_used,
                   const std::string& server_hash,
                   int use_count,
                   size_t dictionary_size)
        : last_used(last_used),
          server_hash(server_hash),
          use_count(use_count),
          dictionary_size(dictionary_size) {}
    DictionaryItem(const DictionaryItem& rhs) = default;
    DictionaryItem& operator=(const DictionaryItem& rhs) = default;
    bool operator<(const DictionaryItem& rhs) const {
      return last_used < rhs.last_used;
    }
  };

#if defined(OS_CHROMEOS)
  // For debugging http://crbug.com/454198; remove when resolved.
  CHECK_EQ(0u, destroyed_);
  CHECK(clock_.get());
#endif

  std::vector<DictionaryItem> stale_dictionary_list;
  size_t recoverable_bytes = 0;
  base::Time stale_boundary(clock_->Now() - base::TimeDelta::FromDays(1));
  for (auto used_it = local_dictionary_info_.begin();
       used_it != local_dictionary_info_.end(); ++used_it) {
    if (used_it->second.last_used < stale_boundary) {
      stale_dictionary_list.push_back(
          DictionaryItem(used_it->second.last_used, used_it->first,
                         used_it->second.use_count, used_it->second.size));
      recoverable_bytes += used_it->second.size;
    }
  }

#if defined(OS_CHROMEOS)
  // For debugging http://crbug.com/454198; remove when resolved.
  CHECK_EQ(0u, destroyed_);
  CHECK(clock_.get());
#endif

  if (total_dictionary_bytes_ + dictionary_text.size() - recoverable_bytes >
      max_total_dictionary_size_) {
    RecordDictionaryFate(DICTIONARY_FATE_FETCH_IGNORED_NO_SPACE);
    net::SdchManager::SdchErrorRecovery(SDCH_DICTIONARY_NO_ROOM);
    net_log.AddEvent(net::NetLog::TYPE_SDCH_DICTIONARY_ERROR,
                     base::Bind(&net::NetLogSdchDictionaryFetchProblemCallback,
                                SDCH_DICTIONARY_NO_ROOM, dictionary_url, true));
    return;
  }

  // Evict from oldest to youngest until we have space.
  std::sort(stale_dictionary_list.begin(), stale_dictionary_list.end());
  size_t avail_bytes = max_total_dictionary_size_ - total_dictionary_bytes_;
  auto stale_it = stale_dictionary_list.begin();
  while (avail_bytes < dictionary_text.size() &&
         stale_it != stale_dictionary_list.end()) {
    manager_->RemoveSdchDictionary(stale_it->server_hash);
    RecordDictionaryEviction(stale_it->use_count,
                             DICTIONARY_FATE_EVICT_FOR_DICT);
    local_dictionary_info_.erase(stale_it->server_hash);
    avail_bytes += stale_it->dictionary_size;
    ++stale_it;
  }
  DCHECK(avail_bytes >= dictionary_text.size());

  std::string server_hash;
  net::SdchProblemCode rv = manager_->AddSdchDictionary(
      dictionary_text, dictionary_url, &server_hash);
  if (rv != net::SDCH_OK) {
    RecordDictionaryFate(DICTIONARY_FATE_FETCH_MANAGER_REFUSED);
    net::SdchManager::SdchErrorRecovery(rv);
    net_log.AddEvent(net::NetLog::TYPE_SDCH_DICTIONARY_ERROR,
                     base::Bind(&net::NetLogSdchDictionaryFetchProblemCallback,
                                rv, dictionary_url, true));
    return;
  }

  RecordDictionaryFate(DICTIONARY_FATE_ADDED);

  DCHECK(local_dictionary_info_.end() ==
         local_dictionary_info_.find(server_hash));
  total_dictionary_bytes_ += dictionary_text.size();
  local_dictionary_info_[server_hash] = DictionaryInfo(
      // Set the time last used to something to avoid thrashing, but not recent,
      // to avoid taking too much time/space with useless dictionaries/one-off
      // visits to web sites.
      clock_->Now() - base::TimeDelta::FromHours(23), dictionary_text.size());

#if defined(OS_CHROMEOS)
  // For debugging http://crbug.com/454198; remove when resolved.
  CHECK_EQ(0u, destroyed_);
  CHECK(clock_.get());
#endif
}

void SdchOwner::OnDictionaryUsed(SdchManager* manager,
                                 const std::string& server_hash) {
  auto it = local_dictionary_info_.find(server_hash);
  DCHECK(local_dictionary_info_.end() != it);

  it->second.last_used = clock_->Now();
  it->second.use_count++;
}

void SdchOwner::OnGetDictionary(net::SdchManager* manager,
                                const GURL& request_url,
                                const GURL& dictionary_url) {
#if defined(OS_CHROMEOS)
  // For debugging http://crbug.com/454198; remove when resolved.
  char url_buf[128];
  if (0u != destroyed_ || !clock_.get()) {
    base::strlcpy(url_buf, request_url.spec().c_str(), arraysize(url_buf));
  }
  base::debug::Alias(url_buf);

  CHECK_EQ(0u, destroyed_);
  CHECK(clock_.get());
#endif

  base::Time stale_boundary(clock_->Now() - base::TimeDelta::FromDays(1));
  size_t avail_bytes = 0;
  for (auto it = local_dictionary_info_.begin();
       it != local_dictionary_info_.end(); ++it) {
    if (it->second.last_used < stale_boundary)
      avail_bytes += it->second.size;
  }

  // Don't initiate the fetch if we wouldn't be able to store any
  // reasonable dictionary.
  // TODO(rdsmith): Maybe do a HEAD request to figure out how much
  // storage we'd actually need?
  if (max_total_dictionary_size_ < (total_dictionary_bytes_ - avail_bytes +
                                    min_space_for_dictionary_fetch_)) {
    RecordDictionaryFate(DICTIONARY_FATE_GET_IGNORED);
    // TODO(rdsmith): Log a net-internals error.  This requires
    // SdchManager to forward the URLRequest that detected the
    // Get-Dictionary header to its observers, which is tricky
    // because SdchManager is layered underneath URLRequest.
    return;
  }

  fetcher_.Schedule(dictionary_url);
}

void SdchOwner::OnClearDictionaries(net::SdchManager* manager) {
  total_dictionary_bytes_ = 0;
  local_dictionary_info_.clear();
  fetcher_.Cancel();
}

void SdchOwner::SetClockForTesting(scoped_ptr<base::Clock> clock) {
  clock_ = clock.Pass();

#if defined(OS_CHROMEOS)
  // For debugging http://crbug.com/454198; remove when resolved.
  CHECK_EQ(0u, destroyed_);
  CHECK(clock_.get());
#endif
}

void SdchOwner::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  DCHECK_NE(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE, level);

  for (auto it = local_dictionary_info_.begin();
       it != local_dictionary_info_.end(); ++it) {
    RecordDictionaryEviction(it->second.use_count,
                             DICTIONARY_FATE_EVICT_FOR_MEMORY);
  }

  // TODO(rdsmith): Make a distinction between moderate and critical
  // memory pressure.
  manager_->ClearData();
}

}  // namespace net
