// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/window_activity_tracker_aura.h"

#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_utils.h"

namespace content {

namespace {
// The time period within which a triggered UI event is considered
// currently active.
const int kTimePeriodUiEventMicros = 100000;  // 100 ms

// Minimum number of user interactions before we consider the user to be in
// interactive mode. The goal is to prevent user interactions to launch
// animated content from causing target playout time flip-flop.
const int kMinUserInteractions = 5;
}  // namespace

WindowActivityTrackerAura::WindowActivityTrackerAura(aura::Window* window)
    : window_(window),
      last_time_ui_event_detected_(base::TimeTicks()),
      ui_events_count_(0),
      weak_factory_(this) {
  if (window_) {
    window_->AddObserver(this);
    window_->AddPreTargetHandler(this);
  }
}

WindowActivityTrackerAura::~WindowActivityTrackerAura() {
  if (window_) {
    window_->RemoveObserver(this);
    window_->RemovePreTargetHandler(this);
  }
}

base::WeakPtr<WindowActivityTracker> WindowActivityTrackerAura::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool WindowActivityTrackerAura::IsUiInteractionActive() const {
  return ui_events_count_ > kMinUserInteractions;
}

void WindowActivityTrackerAura::Reset() {
  ui_events_count_ = 0;
  last_time_ui_event_detected_ = base::TimeTicks();
}

void WindowActivityTrackerAura::OnEvent(ui::Event* event) {
  if (base::TimeTicks::Now() - last_time_ui_event_detected_ >
      base::TimeDelta::FromMicroseconds(kTimePeriodUiEventMicros)) {
    ui_events_count_++;
  }
  last_time_ui_event_detected_ = base::TimeTicks::Now();
}

void WindowActivityTrackerAura::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window_, window);
  window_->RemovePreTargetHandler(this);
  window_->RemoveObserver(this);
  window_ = nullptr;
}

}  // namespace content
