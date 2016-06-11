// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Set of PhotoCapabilities read from the different VideoCapture Devices.
 **/
@JNINamespace("media")
class PhotoCapabilities {
    public final int maxZoom;
    public final int minZoom;
    public final int currentZoom;

    PhotoCapabilities(int maxZoom, int minZoom, int currentZoom) {
        this.maxZoom = maxZoom;
        this.minZoom = minZoom;
        this.currentZoom = currentZoom;
    }

    @CalledByNative
    public int getMinZoom() {
        return minZoom;
    }
    @CalledByNative
    public int getMaxZoom() {
        return maxZoom;
    }
    @CalledByNative
    public int getCurrentZoom() {
        return currentZoom;
    }
}
