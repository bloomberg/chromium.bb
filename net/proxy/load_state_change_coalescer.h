// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_LOAD_STATE_CHANGE_COALESCER_H_
#define NET_PROXY_LOAD_STATE_CHANGE_COALESCER_H_

#include "base/cancelable_callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/load_states.h"

namespace net {

// A class that coalesces LoadState changes. When a new LoadState is reported,
// it becomes the pending LoadState and is queued for |timeout|. If the timeout
// elapses without another new LoadState, the pending LoadState becomes the
// committed LoadState and the callback is called with that LoadState. If a new
// LoadState is reported before the timeout has elapsed, the pending LoadState
// is discarded and the new LoadState becomes the new pending LoadState, unless
// it's the same as the committed LoadState, in which case no pending LoadState
// is queued.
class LoadStateChangeCoalescer {
 public:
  LoadStateChangeCoalescer(const base::Callback<void(LoadState)>& callback,
                           const base::TimeDelta& timeout,
                           LoadState initial_load_state);
  ~LoadStateChangeCoalescer();

  // Adds a LoadState change to the pipeline. If it isn't coalesced, |callback_|
  // will be called with |load_state| after |timeout_|.
  void LoadStateChanged(LoadState load_state);

 private:
  void SendLoadStateChanged();

  // The callback to call to report a LoadState change.
  const base::Callback<void(LoadState)> callback_;

  // The amount of time for which a LoadState change can be coalesced.
  const base::TimeDelta timeout_;

  base::OneShotTimer<LoadStateChangeCoalescer> timer_;
  LoadState committed_load_state_;
  LoadState pending_load_state_;

  DISALLOW_COPY_AND_ASSIGN(LoadStateChangeCoalescer);
};

}  // namespace net

#endif  // NET_PROXY_LOAD_STATE_CHANGE_COALESCER_H_
