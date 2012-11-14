// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "cc/settings.h"
#include "cc/switches.h"

namespace {
static bool s_settingsInitialized = false;

static bool s_perTilePaintingEnabled = false;
static bool s_partialSwapEnabled = false;
static bool s_acceleratedAnimationEnabled = false;
static bool s_pageScalePinchZoomEnabled = false;
static bool s_backgroundColorInsteadOfCheckerboard = false;
static bool s_traceOverdraw = false;

void reset()
{
    s_settingsInitialized = true;

    s_perTilePaintingEnabled = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kEnablePerTilePainting);
    s_partialSwapEnabled = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kEnablePartialSwap);
    s_acceleratedAnimationEnabled = !CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kDisableThreadedAnimation);
    s_pageScalePinchZoomEnabled = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kEnablePinchInCompositor);
    s_backgroundColorInsteadOfCheckerboard = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kBackgroundColorInsteadOfCheckerboard);
    s_traceOverdraw = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kTraceOverdraw);
}

}

namespace cc {

bool Settings::perTilePaintingEnabled()
{
    if (!s_settingsInitialized)
        reset();
    return s_perTilePaintingEnabled;
}

bool Settings::partialSwapEnabled()
{
    if (!s_settingsInitialized)
        reset();
    return s_partialSwapEnabled;
}

bool Settings::acceleratedAnimationEnabled()
{
    if (!s_settingsInitialized)
        reset();
    return s_acceleratedAnimationEnabled;
}

bool Settings::pageScalePinchZoomEnabled()
{
    if (!s_settingsInitialized)
        reset();
    return s_pageScalePinchZoomEnabled;
}

bool Settings::backgroundColorInsteadOfCheckerboard()
{
    if (!s_settingsInitialized)
        reset();
    return s_backgroundColorInsteadOfCheckerboard;
}

bool Settings::traceOverdraw()
{
    if (!s_settingsInitialized)
        reset();
    return s_traceOverdraw;
}

void Settings::resetForTest()
{
    reset();
}

void Settings::setPartialSwapEnabled(bool enabled)
{
    if (!s_settingsInitialized)
        reset();
    s_partialSwapEnabled = enabled;
}

void Settings::setPerTilePaintingEnabled(bool enabled)
{
    if (!s_settingsInitialized)
        reset();
    s_partialSwapEnabled = enabled;
}

void Settings::setAcceleratedAnimationEnabled(bool enabled)
{
    if (!s_settingsInitialized)
        reset();
    s_acceleratedAnimationEnabled = enabled;
}

void Settings::setPageScalePinchZoomEnabled(bool enabled)
{
    if (!s_settingsInitialized)
        reset();
    s_pageScalePinchZoomEnabled = enabled;
}

}  // namespace cc
