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
//
// This file implements a mock location provider and the factory functions for
// creating various types of location provider.

// TODO(joth): port to chromium
#if 0

#include "gears/geolocation/location_provider.h"

#include <assert.h>
#include "gears/base/common/scoped_refptr.h"  // For RefCount

void LocationProviderBase::RegisterListener(ListenerInterface *listener,
                                            bool request_address) {
  assert(listener);
  MutexLock lock(&listeners_mutex_);
  ListenerMap::iterator iter = listeners_.find(listener);
  if (iter == listeners_.end()) {
    std::pair<ListenerMap::iterator, bool> result =
        listeners_.insert(
        std::make_pair(listener,
        std::make_pair(request_address, new RefCount())));
    assert(result.second);
    iter = result.first;
  }
  RefCount *count = iter->second.second;
  assert(count);
  count->Ref();
}

void LocationProviderBase::UnregisterListener(ListenerInterface *listener) {
  assert(listener);
  MutexLock lock(&listeners_mutex_);
  ListenerMap::iterator iter = listeners_.find(listener);
  if (iter != listeners_.end()) {
    RefCount *count = iter->second.second;
    assert(count);
    if (count->Unref()) {
      delete count;
      listeners_.erase(iter);
    }
  }
}

void LocationProviderBase::UpdateListeners() {
  MutexLock lock(&listeners_mutex_);
  for (ListenerMap::const_iterator iter = listeners_.begin();
       iter != listeners_.end();
       ++iter) {
    iter->first->LocationUpdateAvailable(this);
  }
}

void LocationProviderBase::InformListenersOfMovement() {
  MutexLock lock(&listeners_mutex_);
  for (ListenerMap::const_iterator iter = listeners_.begin();
       iter != listeners_.end();
       ++iter) {
    iter->first->MovementDetected(this);
  }
}

LocationProviderBase::ListenerMap *LocationProviderBase::GetListeners() {
  return &listeners_;
}

Mutex *LocationProviderBase::GetListenersMutex() {
  return &listeners_mutex_;
}

// Win32, Linux and OSX do not have a GPS location provider.
#if (defined(WIN32) && !defined(OS_WINCE)) || \
    defined(LINUX) || \
    defined(OS_MACOSX)

LocationProviderBase *NewGpsLocationProvider(
    BrowsingContext *browsing_context,
    const std::string16 &reverse_geocode_url,
    const std::string16 &host_name,
    const std::string16 &address_language) {
  return NULL;
}

#endif

#endif  // if 0
