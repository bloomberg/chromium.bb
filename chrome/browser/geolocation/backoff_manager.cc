// Copyright 2008, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// TODO(joth): port to chromium
#if 0

#include "gears/geolocation/backoff_manager.h"

#include <assert.h>

// The baseline minimum period between network requests.
static const int kBaselineMinimumRequestInterval = 1000 * 5;  // 5 seconds
// The upper limit of the minimum period between network requests.
static const int kMinimumRequestIntervalLimit = 1000 * 60 * 60 * 3;  // 3 hours


// static
BackoffManager::ServerMap BackoffManager::servers_;

// static
Mutex BackoffManager::servers_mutex_;

// static
void BackoffManager::ReportRequest(const std::string16 &url) {
  MutexLock lock(&servers_mutex_);
  ServerMap::iterator iter = servers_.find(url);
  if (iter != servers_.end()) {
    iter->second.first = GetCurrentTimeMillis();
  } else {
    servers_[url] = std::make_pair(GetCurrentTimeMillis(),
                                   kBaselineMinimumRequestInterval);
  }
}

// static
int64 BackoffManager::ReportResponse(const std::string16 &url,
                                     bool server_error) {
  // Use exponential back-off on server error.
  MutexLock lock(&servers_mutex_);
  ServerMap::iterator iter = servers_.find(url);
  assert(iter != servers_.end());
  int64 *interval = &iter->second.second;
  if (server_error) {
    if (*interval < kMinimumRequestIntervalLimit) {
      // Increase interval by between 90% and 110%.
      srand(static_cast<unsigned int>(GetCurrentTimeMillis()));
      double increment_proportion = 0.9 + 0.2 * rand() / RAND_MAX;
      int64 increment = static_cast<int64>(*interval * increment_proportion);
      if (increment > kMinimumRequestIntervalLimit - *interval) {
        *interval = kMinimumRequestIntervalLimit;
      } else {
        *interval += increment;
      }
    }
  } else {
    *interval = kBaselineMinimumRequestInterval;
  }
  return iter->second.first + *interval;
}

#endif  // if 0
