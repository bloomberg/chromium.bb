// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/invalidation_auth_provider.h"

namespace invalidation {

InvalidationAuthProvider::Observer::~Observer() {}

InvalidationAuthProvider::~InvalidationAuthProvider() {}

void InvalidationAuthProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InvalidationAuthProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

InvalidationAuthProvider::InvalidationAuthProvider() {}

void InvalidationAuthProvider::FireInvalidationAuthLogout() {
  FOR_EACH_OBSERVER(Observer, observers_, OnInvalidationAuthLogout());
}

}  // namespace invalidation
