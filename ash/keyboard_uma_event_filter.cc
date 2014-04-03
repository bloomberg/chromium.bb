// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard_uma_event_filter.h"

#include "base/metrics/histogram.h"
#include "ui/events/event.h"

namespace {

// This threshold is used to drop keystrokes that are more than some time apart.
// These keystrokes are dropped to avoid recording outliers, as well as pauses
// between actual segments of typing.
const int kKeystrokeThresholdInSeconds = 5;

}

namespace ash {

KeyboardUMAEventFilter::KeyboardUMAEventFilter() {}

KeyboardUMAEventFilter::~KeyboardUMAEventFilter() {}

void KeyboardUMAEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  // This is a rough approximation, so assume that each key release is the
  // result of a typed key.
  if (event->type() != ui::ET_KEY_RELEASED)
    return;

  // Reset the timer on non-character keystrokes.
  if (!isprint(event->GetCharacter())) {
    last_keystroke_time_ = base::TimeDelta();
    return;
  }

  if (last_keystroke_time_.ToInternalValue() == 0) {
    last_keystroke_time_ = event->time_stamp();
    return;
  }

  base::TimeDelta delta = event->time_stamp() - last_keystroke_time_;
  if (delta < base::TimeDelta::FromSeconds(kKeystrokeThresholdInSeconds))
    UMA_HISTOGRAM_TIMES("Keyboard.KeystrokeDeltas", delta);

  last_keystroke_time_ = event->time_stamp();
}

}  // namespace ash
