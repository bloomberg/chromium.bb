// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCSettings_h
#define CCSettings_h

#include "IntSize.h"

namespace cc {

// This file is for settings that apply to all compositors.  Add settings to
// CCLayerTreeSettings if a ui and renderer compositor might not want the same
// setting.

class CCSettings {
public:
    static bool perTilePaintingEnabled();
    static bool partialSwapEnabled();
    static bool acceleratedAnimationEnabled();
    static bool pageScalePinchZoomEnabled();

    static bool jankInsteadOfCheckerboard();
    static bool backgroundColorInsteadOfCheckerboard();

    // These setters should only be used on the main thread before the layer
    // renderer is initialized.
    static void setPerTilePaintingEnabled(bool);
    static void setPartialSwapEnabled(bool);
    static void setAcceleratedAnimationEnabled(bool);
    static void setPageScalePinchZoomEnabled(bool);

    // These settings are meant to be set only once, and only read thereafter.
    // This function is only for resetting settings in tests.
    static void reset();
};

} // namespace cc

#endif // CCSettings_h
