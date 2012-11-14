// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SETTINGS_H_
#define CC_SETTINGS_H_

#include "cc/cc_export.h"

namespace cc {

// This file is for settings that apply to all compositors.  Add settings to
// LayerTreeSettings if a ui and renderer compositor might not want the same
// setting.

class CC_EXPORT Settings {
public:
    static bool perTilePaintingEnabled();
    static bool partialSwapEnabled();
    static bool acceleratedAnimationEnabled();
    static bool pageScalePinchZoomEnabled();
    static bool backgroundColorInsteadOfCheckerboard();
    static bool traceOverdraw();

    static void setPartialSwapEnabled(bool);
    static void setPerTilePaintingEnabled(bool);
    static void setAcceleratedAnimationEnabled(bool);
    static void setPageScalePinchZoomEnabled(bool);

    static void resetForTest();
};

} // namespace cc

#endif  // CC_SETTINGS_H_
