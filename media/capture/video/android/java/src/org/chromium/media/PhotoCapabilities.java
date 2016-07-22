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
    public final int maxIso;
    public final int minIso;
    public final int currentIso;
    public final int maxHeight;
    public final int minHeight;
    public final int currentHeight;
    public final int maxWidth;
    public final int minWidth;
    public final int currentWidth;
    public final int maxZoom;
    public final int minZoom;
    public final int currentZoom;
    public final boolean autoFocusInUse;

    PhotoCapabilities(int maxIso, int minIso, int currentIso, int maxHeight, int minHeight,
            int currentHeight, int maxWidth, int minWidth, int currentWidth, int maxZoom,
            int minZoom, int currentZoom, boolean autoFocusInUse) {
        this.maxIso = maxIso;
        this.minIso = minIso;
        this.currentIso = currentIso;
        this.maxHeight = maxHeight;
        this.minHeight = minHeight;
        this.currentHeight = currentHeight;
        this.maxWidth = maxWidth;
        this.minWidth = minWidth;
        this.currentWidth = currentWidth;
        this.maxZoom = maxZoom;
        this.minZoom = minZoom;
        this.currentZoom = currentZoom;
        this.autoFocusInUse = autoFocusInUse;
    }

    @CalledByNative
    public int getMinIso() {
        return minIso;
    }

    @CalledByNative
    public int getMaxIso() {
        return maxIso;
    }

    @CalledByNative
    public int getCurrentIso() {
        return currentIso;
    }

    @CalledByNative
    public int getMinHeight() {
        return minHeight;
    }

    @CalledByNative
    public int getMaxHeight() {
        return maxHeight;
    }

    @CalledByNative
    public int getCurrentHeight() {
        return currentHeight;
    }

    @CalledByNative
    public int getMinWidth() {
        return minWidth;
    }

    @CalledByNative
    public int getMaxWidth() {
        return maxWidth;
    }

    @CalledByNative
    public int getCurrentWidth() {
        return currentWidth;
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

    @CalledByNative
    public boolean getAutoFocusInUse() {
        return autoFocusInUse;
    }
}
