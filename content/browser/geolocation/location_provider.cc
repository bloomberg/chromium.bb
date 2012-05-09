// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/location_provider.h"

#include "base/logging.h"

LocationProviderBase::LocationProviderBase() {
}

LocationProviderBase::~LocationProviderBase() {
  DCHECK(CalledOnValidThread());
}

void LocationProviderBase::RegisterListener(ListenerInterface* listener) {
  DCHECK(CalledOnValidThread());
  DCHECK(listener);
  std::pair<ListenerMap::iterator, bool> result =
      listeners_.insert(std::make_pair(listener, 0));
  DCHECK(result.first != listeners_.end());

  int& ref_count = result.first->second;
  const bool& is_new = result.second;
  ++ref_count;

  // Check the post condition...
  if (is_new) {
    DCHECK(ref_count == 1);
  } else {
    DCHECK(ref_count > 1);
  }
}

void LocationProviderBase::UnregisterListener(ListenerInterface *listener) {
  DCHECK(CalledOnValidThread());
  DCHECK(listener);
  ListenerMap::iterator iter = listeners_.find(listener);
  if (iter != listeners_.end()) {
    if (--iter->second == 0) {
      listeners_.erase(iter);
    }
  }
}

bool LocationProviderBase::has_listeners() const {
  return !listeners_.empty();
}

void LocationProviderBase::UpdateListeners() {
  DCHECK(CalledOnValidThread());
  for (ListenerMap::const_iterator iter = listeners_.begin();
       iter != listeners_.end();
       ++iter) {
    iter->first->LocationUpdateAvailable(this);
  }
}

#if !defined(OS_LINUX) && !defined(OS_MACOSX) && !defined(OS_WIN) && !defined OS_ANDROID
LocationProviderBase* NewSystemLocationProvider() {
  return NULL;
}
#endif
