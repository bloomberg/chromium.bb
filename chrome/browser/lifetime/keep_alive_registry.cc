// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/keep_alive_registry.h"

#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/keep_alive_types.h"

////////////////////////////////////////////////////////////////////////////////
// Public methods

// static
KeepAliveRegistry* KeepAliveRegistry::GetInstance() {
  return base::Singleton<KeepAliveRegistry>::get();
}

bool KeepAliveRegistry::WillKeepAlive() const {
  return registered_count_ > 0;
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

KeepAliveRegistry::KeepAliveRegistry() : registered_count_(0) {}

KeepAliveRegistry::~KeepAliveRegistry() {
  DCHECK_EQ(0, registered_count_);
  DCHECK_EQ(0u, registered_keep_alives_.size());
}

void KeepAliveRegistry::Register(KeepAliveOrigin origin) {
  if (!WillKeepAlive())
    chrome::IncrementKeepAliveCount();

  ++registered_keep_alives_[origin];
  ++registered_count_;
}

void KeepAliveRegistry::Unregister(KeepAliveOrigin origin) {
  --registered_count_;
  DCHECK_GE(registered_count_, 0);

  int new_count = --registered_keep_alives_[origin];
  DCHECK_GE(registered_keep_alives_[origin], 0);
  if (new_count == 0)
    registered_keep_alives_.erase(origin);

  if (!WillKeepAlive())
    chrome::DecrementKeepAliveCount();
}
