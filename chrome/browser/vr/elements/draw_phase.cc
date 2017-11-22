// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/draw_phase.h"

#include "base/macros.h"

namespace vr {

namespace {

static const char* g_draw_phase_strings[] = {
    "kPhaseNone",         "kPhaseBackground", "kPhaseOverlayBackground",
    "kPhaseFloorCeiling", "kPhaseForeground", "kPhaseOverlayForeground",
};

static_assert(
    kNumDrawPhases + 1 == arraysize(g_draw_phase_strings),
    "Mismatch between the DrawPhase enum and the corresponding strings");

}  // namespace

std::string DrawPhaseToString(DrawPhase phase) {
  return g_draw_phase_strings[phase];
}

}  // namespace vr
