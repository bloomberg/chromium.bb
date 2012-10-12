// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/command_line.h"
#include "CCSettings.h"
#include "cc/switches.h"

namespace {
static bool s_perTilePaintingEnabled = false;
static bool s_partialSwapEnabled = false;
static bool s_acceleratedAnimationEnabled = false;
static bool s_pageScalePinchZoomEnabled = false;
} // namespace

namespace cc {

bool CCSettings::perTilePaintingEnabled() { return s_perTilePaintingEnabled; }
void CCSettings::setPerTilePaintingEnabled(bool enabled) { s_perTilePaintingEnabled = enabled; }

bool CCSettings::partialSwapEnabled() { return s_partialSwapEnabled; }
void CCSettings::setPartialSwapEnabled(bool enabled) { s_partialSwapEnabled = enabled; }

bool CCSettings::acceleratedAnimationEnabled() { return s_acceleratedAnimationEnabled; }
void CCSettings::setAcceleratedAnimationEnabled(bool enabled) { s_acceleratedAnimationEnabled = enabled; }

bool CCSettings::pageScalePinchZoomEnabled() { return s_pageScalePinchZoomEnabled; }
void CCSettings::setPageScalePinchZoomEnabled(bool enabled) { s_pageScalePinchZoomEnabled = enabled; }

bool CCSettings::jankInsteadOfCheckerboard()
{
    return CommandLine::ForCurrentProcess()->HasSwitch(switches::kJankInsteadOfCheckerboard);
}

bool CCSettings::backgroundColorInsteadOfCheckerboard()
{
    return CommandLine::ForCurrentProcess()->HasSwitch(switches::kBackgroundColorInsteadOfCheckerboard);
}

void CCSettings::reset()
{
    s_perTilePaintingEnabled = false;
    s_partialSwapEnabled = false;
    s_acceleratedAnimationEnabled = false;
    s_pageScalePinchZoomEnabled = false;
}

} // namespace cc
