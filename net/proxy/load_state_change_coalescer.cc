// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/load_state_change_coalescer.h"

namespace net {

LoadStateChangeCoalescer::LoadStateChangeCoalescer(
    const base::Callback<void(LoadState)>& callback,
    const base::TimeDelta& timeout,
    LoadState initial_load_state)
    : callback_(callback),
      timeout_(timeout),
      committed_load_state_(initial_load_state),
      pending_load_state_(initial_load_state) {
}

void LoadStateChangeCoalescer::LoadStateChanged(LoadState load_state) {
  if (load_state == committed_load_state_) {
    timer_.Stop();
    return;
  }
  pending_load_state_ = load_state;
  timer_.Start(FROM_HERE, timeout_, this,
               &LoadStateChangeCoalescer::SendLoadStateChanged);
}

LoadStateChangeCoalescer::~LoadStateChangeCoalescer() = default;

void LoadStateChangeCoalescer::SendLoadStateChanged() {
  committed_load_state_ = pending_load_state_;
  callback_.Run(pending_load_state_);
}

}  // namespace net
