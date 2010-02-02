// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a mock location provider and the factory functions for
// creating various types of location provider.

#include "chrome/browser/geolocation/location_provider.h"

#include <assert.h>

LocationProviderBase::LocationProviderBase()
    : client_loop_(MessageLoop::current()) {
}

LocationProviderBase::~LocationProviderBase() {
  DCHECK_EQ(client_loop_, MessageLoop::current());
}

void LocationProviderBase::RegisterListener(ListenerInterface* listener) {
  DCHECK(listener);
  if (RunInClientThread(FROM_HERE,
                        &LocationProviderBase::RegisterListener, listener))
    return;
  ListenerMap::iterator iter = listeners_.find(listener);
  if (iter == listeners_.end()) {
    std::pair<ListenerMap::iterator, bool> result =
        listeners_.insert(std::make_pair(listener, 0));
    DCHECK(result.second);
    iter = result.first;
  }
  ++iter->second;
}

void LocationProviderBase::UnregisterListener(ListenerInterface *listener) {
  DCHECK(listener);
  if (RunInClientThread(FROM_HERE,
                        &LocationProviderBase::UnregisterListener, listener))
    return;
  ListenerMap::iterator iter = listeners_.find(listener);
  if (iter != listeners_.end()) {
    if (--iter->second == 0) {
      listeners_.erase(iter);
    }
  }
}

void LocationProviderBase::UpdateListeners() {
  // Currently we required location provider implementations to make
  // notifications from the client thread. This could be relaxed if needed.
  CheckRunningInClientLoop();
  for (ListenerMap::const_iterator iter = listeners_.begin();
       iter != listeners_.end();
       ++iter) {
    iter->first->LocationUpdateAvailable(this);
  }
}

void LocationProviderBase::InformListenersOfMovement() {
  // Currently we required location provider implementations to make
  // notifications from the client thread. This could be relaxed if needed.
  CheckRunningInClientLoop();
  for (ListenerMap::const_iterator iter = listeners_.begin();
       iter != listeners_.end();
       ++iter) {
    iter->first->MovementDetected(this);
  }
}

void LocationProviderBase::CheckRunningInClientLoop() {
  DCHECK_EQ(MessageLoop::current(), client_loop());
}

// Currently no platforms have a GPS location provider.
LocationProviderBase* NewGpsLocationProvider() {
  return NULL;
}
