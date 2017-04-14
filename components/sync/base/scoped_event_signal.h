// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SCOPED_EVENT_SIGNAL_H_
#define COMPONENTS_SYNC_BASE_SCOPED_EVENT_SIGNAL_H_

#include "base/macros.h"

namespace base {
class WaitableEvent;
}

namespace syncer {

// An object which signals a WaitableEvent when it is deleted. Used to wait for
// a task to run or be abandoned.
class ScopedEventSignal {
 public:
  // |event| will be signaled in the destructor.
  explicit ScopedEventSignal(base::WaitableEvent* event);

  // This ScopedEventSignal will signal |other|'s WaitableEvent in its
  // destructor. |other| will not signal anything in its destructor. The
  // assignment operator cannot be used if this ScopedEventSignal's
  // WaitableEvent hasn't been moved to another ScopedEventSignal.
  ScopedEventSignal(ScopedEventSignal&& other);
  ScopedEventSignal& operator=(ScopedEventSignal&& other);

  ~ScopedEventSignal();

 private:
  base::WaitableEvent* event_;

  DISALLOW_COPY_AND_ASSIGN(ScopedEventSignal);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_SCOPED_EVENT_SIGNAL_H_
