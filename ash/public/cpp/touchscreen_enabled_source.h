// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TOUCHSCREEN_ENABLED_SOURCE_H_
#define ASH_PUBLIC_CPP_TOUCHSCREEN_ENABLED_SOURCE_H_

namespace ash {

// Different sources for requested touchscreen enabled/disabled state.
enum class TouchscreenEnabledSource {
  // User-requested state set via a debug accelerator and stored in a pref.
  USER_PREF,
  // Transient global state used to disable touchscreen via power button.
  GLOBAL,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TOUCHSCREEN_ENABLED_SOURCE_H_
