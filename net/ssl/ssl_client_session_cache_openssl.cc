// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_client_session_cache_openssl.h"

#include <utility>

#include "base/logging.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"

namespace net {

SSLClientSessionCacheOpenSSL::SSLClientSessionCacheOpenSSL(const Config& config)
    : clock_(new base::DefaultClock),
      config_(config),
      cache_(config.max_entries),
      lookups_since_flush_(0) {
}

SSLClientSessionCacheOpenSSL::~SSLClientSessionCacheOpenSSL() {
  // TODO(davidben): The session cache is currently a singleton, so it is
  // destroyed on a different thread than the one it's created on. When
  // https://crbug.com/458365 is fixed, this will no longer be an issue.
  thread_checker_.DetachFromThread();

  Flush();
}

size_t SSLClientSessionCacheOpenSSL::size() const {
  return cache_.size();
}

SSL_SESSION* SSLClientSessionCacheOpenSSL::Lookup(
    const std::string& cache_key) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Expire stale sessions.
  lookups_since_flush_++;
  if (lookups_since_flush_ >= config_.expiration_check_count) {
    lookups_since_flush_ = 0;
    FlushExpiredSessions();
  }

  CacheEntryMap::iterator iter = cache_.Get(cache_key);
  if (iter == cache_.end())
    return nullptr;
  if (IsExpired(iter->second, clock_->Now())) {
    cache_.Erase(iter);
    return nullptr;
  }
  return iter->second->session.get();
}

void SSLClientSessionCacheOpenSSL::Insert(const std::string& cache_key,
                                          SSL_SESSION* session) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Make a new entry.
  CacheEntry* entry = new CacheEntry;
  entry->session.reset(SSL_SESSION_up_ref(session));
  entry->creation_time = clock_->Now();

  // Takes ownership.
  cache_.Put(cache_key, entry);
}

void SSLClientSessionCacheOpenSSL::Flush() {
  DCHECK(thread_checker_.CalledOnValidThread());

  cache_.Clear();
}

void SSLClientSessionCacheOpenSSL::SetClockForTesting(
    scoped_ptr<base::Clock> clock) {
  DCHECK(thread_checker_.CalledOnValidThread());

  clock_ = clock.Pass();
}

SSLClientSessionCacheOpenSSL::CacheEntry::CacheEntry() {
}

SSLClientSessionCacheOpenSSL::CacheEntry::~CacheEntry() {
}

bool SSLClientSessionCacheOpenSSL::IsExpired(
    SSLClientSessionCacheOpenSSL::CacheEntry* entry,
    const base::Time& now) {
  return now < entry->creation_time ||
         entry->creation_time + config_.timeout < now;
}

void SSLClientSessionCacheOpenSSL::FlushExpiredSessions() {
  base::Time now = clock_->Now();
  CacheEntryMap::iterator iter = cache_.begin();
  while (iter != cache_.end()) {
    if (IsExpired(iter->second, now)) {
      iter = cache_.Erase(iter);
    } else {
      ++iter;
    }
  }
}

}  // namespace net
