// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCSettings_h
#define CCSettings_h

#include "IntSize.h"

namespace cc {

class CCSettings {
public:
    static bool perTilePaintingEnabled();
    static bool partialSwapEnabled();
    static bool acceleratedAnimationEnabled();

    // These setters should only be used on the main thread before the layer
    // renderer is initialized.
    static void setPerTilePaintingEnabled(bool);
    static void setPartialSwapEnabled(bool);
    static void setAcceleratedAnimationEnabled(bool);

    // These settings are meant to be set only once, and only read thereafter.
    // This function is only for resetting settings in tests.
    static void reset();
};

} // namespace cc

#endif // CCSettings_h
