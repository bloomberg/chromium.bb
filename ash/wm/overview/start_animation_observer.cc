// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/start_animation_observer.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace ash {

ForceDelayObserver::ForceDelayObserver(base::TimeDelta delay) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ForceDelayObserver::Finish,
                     weak_ptr_factory_.GetWeakPtr()),
      delay);
}

ForceDelayObserver::~ForceDelayObserver() = default;

void ForceDelayObserver::SetOwner(OverviewDelegate* owner) {
  owner_ = owner;
}

void ForceDelayObserver::Shutdown() {
  owner_ = nullptr;
}

void ForceDelayObserver::Finish() {
  if (owner_)
    owner_->RemoveAndDestroyStartAnimationObserver(this);
}

StartAnimationObserver::StartAnimationObserver() = default;

StartAnimationObserver::~StartAnimationObserver() = default;

void StartAnimationObserver::OnImplicitAnimationsCompleted() {
  if (owner_)
    owner_->RemoveAndDestroyStartAnimationObserver(this);
}

void StartAnimationObserver::SetOwner(OverviewDelegate* owner) {
  DCHECK(!owner_);
  owner_ = owner;
}

void StartAnimationObserver::Shutdown() {
  owner_ = nullptr;
}

}  // namespace ash
