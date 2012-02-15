// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_dispatcher.h"

namespace ash {

AcceleratorDispatcher::AcceleratorDispatcher(
    MessageLoop::Dispatcher* nested_dispatcher, aura::Window* associated_window)
    : nested_dispatcher_(nested_dispatcher),
      associated_window_(associated_window) {
  DCHECK(nested_dispatcher_);
  associated_window_->AddObserver(this);
}

AcceleratorDispatcher::~AcceleratorDispatcher() {
  if (associated_window_)
    associated_window_->RemoveObserver(this);
}

void AcceleratorDispatcher::OnWindowDestroying(aura::Window* window) {
  if (associated_window_ == window)
    associated_window_ = NULL;
}

}  // namespace ash
