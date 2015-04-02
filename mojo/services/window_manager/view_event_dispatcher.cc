// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/view_event_dispatcher.h"

#include "mojo/services/window_manager/view_target.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view.h"

namespace window_manager {

ViewEventDispatcher::ViewEventDispatcher()
    : event_dispatch_target_(nullptr),
      old_dispatch_target_(nullptr) {
}

ViewEventDispatcher::~ViewEventDispatcher() {}

void ViewEventDispatcher::SetRootViewTarget(ViewTarget* root_view_target) {
  root_view_target_ = root_view_target;
}

ui::EventTarget* ViewEventDispatcher::GetRootTarget() {
  return root_view_target_;
}

void ViewEventDispatcher::OnEventProcessingStarted(ui::Event* event) {
}

bool ViewEventDispatcher::CanDispatchToTarget(ui::EventTarget* target) {
  return event_dispatch_target_ == target;
}

ui::EventDispatchDetails ViewEventDispatcher::PreDispatchEvent(
    ui::EventTarget* target,
    ui::Event* event) {
  // TODO(erg): PreDispatch in aura::WindowEventDispatcher does many, many
  // things. It, and the functions split off for different event types, are
  // most of the file.
  old_dispatch_target_ = event_dispatch_target_;
  event_dispatch_target_ = static_cast<ViewTarget*>(target);
  return ui::EventDispatchDetails();
}

ui::EventDispatchDetails ViewEventDispatcher::PostDispatchEvent(
    ui::EventTarget* target,
    const ui::Event& event) {
  // TODO(erg): Not at all as long as PreDispatchEvent, but still missing core
  // details.
  event_dispatch_target_ = old_dispatch_target_;
  old_dispatch_target_ = nullptr;
  return ui::EventDispatchDetails();
}

}  // namespace window_manager
