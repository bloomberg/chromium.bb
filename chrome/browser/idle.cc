// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"



void IdleStateCallback(IdleState* return_state, base::WaitableEvent* done,
                       IdleState state) {
  *return_state = state;
  done->Signal();
}

IdleState CalculateIdleStateSync(unsigned int idle_threshold) {
  IdleState return_state;
  base::WaitableEvent done(true, false);
  CalculateIdleState(idle_threshold, base::Bind(&IdleStateCallback,
                                                &return_state, &done));
  done.Wait();
  return return_state;
}
