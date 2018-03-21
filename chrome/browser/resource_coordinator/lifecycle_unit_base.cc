// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"

#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"

namespace resource_coordinator {

LifecycleUnitBase::LifecycleUnitBase() = default;

LifecycleUnitBase::~LifecycleUnitBase() = default;

int32_t LifecycleUnitBase::GetID() const {
  return id_;
}

LifecycleUnit::State LifecycleUnitBase::GetState() const {
  return state_;
}

void LifecycleUnitBase::AddObserver(LifecycleUnitObserver* observer) {
  observers_.AddObserver(observer);
}

void LifecycleUnitBase::RemoveObserver(LifecycleUnitObserver* observer) {
  observers_.RemoveObserver(observer);
}

void LifecycleUnitBase::SetState(State state) {
  if (state == state_)
    return;
  state_ = state;
  for (auto& observer : observers_)
    observer.OnLifecycleUnitStateChanged(this);
}

void LifecycleUnitBase::OnLifecycleUnitVisibilityChanged(
    content::Visibility visibility) {
  for (auto& observer : observers_)
    observer.OnLifecycleUnitVisibilityChanged(this, visibility);
}

void LifecycleUnitBase::OnLifecycleUnitDestroyed() {
  for (auto& observer : observers_)
    observer.OnLifecycleUnitDestroyed(this);
}

int32_t LifecycleUnitBase::next_id_ = 0;

}  // namespace resource_coordinator
