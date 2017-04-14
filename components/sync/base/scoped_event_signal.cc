// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/scoped_event_signal.h"

#include "base/logging.h"
#include "base/synchronization/waitable_event.h"

namespace syncer {

ScopedEventSignal::ScopedEventSignal(base::WaitableEvent* event)
    : event_(event) {
  DCHECK(event_);
}

ScopedEventSignal::ScopedEventSignal(ScopedEventSignal&& other)
    : event_(other.event_) {
  other.event_ = nullptr;
}

ScopedEventSignal& ScopedEventSignal::operator=(ScopedEventSignal&& other) {
  DCHECK(!event_);
  event_ = other.event_;
  other.event_ = nullptr;
  return *this;
}

ScopedEventSignal::~ScopedEventSignal() {
  if (event_)
    event_->Signal();
}

}  // namespace syncer
