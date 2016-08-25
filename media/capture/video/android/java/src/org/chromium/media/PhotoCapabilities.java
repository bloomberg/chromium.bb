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
    public final int focusMode;
    public final int exposureMode;

    PhotoCapabilities(int maxIso, int minIso, int currentIso, int maxHeight, int minHeight,
            int currentHeight, int maxWidth, int minWidth, int currentWidth, int maxZoom,
            int minZoom, int currentZoom, int focusMode, int exposureMode) {
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
        this.focusMode = focusMode;
        this.exposureMode = exposureMode;
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
    public int getFocusMode() {
        return focusMode;
    }

    @CalledByNative
    public int getExposureMode() {
        return exposureMode;
    }

    public static class Builder {
        public int maxIso;
        public int minIso;
        public int currentIso;
        public int maxHeight;
        public int minHeight;
        public int currentHeight;
        public int maxWidth;
        public int minWidth;
        public int currentWidth;
        public int maxZoom;
        public int minZoom;
        public int currentZoom;
        public int focusMode;
        public int exposureMode;

        public Builder() {}

        public Builder setMaxIso(int maxIso) {
            this.maxIso = maxIso;
            return this;
        }

        public Builder setMinIso(int minIso) {
            this.minIso = minIso;
            return this;
        }

        public Builder setCurrentIso(int currentIso) {
            this.currentIso = currentIso;
            return this;
        }

        public Builder setMaxHeight(int maxHeight) {
            this.maxHeight = maxHeight;
            return this;
        }

        public Builder setMinHeight(int minHeight) {
            this.minHeight = minHeight;
            return this;
        }

        public Builder setCurrentHeight(int currentHeight) {
            this.currentHeight = currentHeight;
            return this;
        }

        public Builder setMaxWidth(int maxWidth) {
            this.maxWidth = maxWidth;
            return this;
        }

        public Builder setMinWidth(int minWidth) {
            this.minWidth = minWidth;
            return this;
        }

        public Builder setCurrentWidth(int currentWidth) {
            this.currentWidth = currentWidth;
            return this;
        }

        public Builder setMaxZoom(int maxZoom) {
            this.maxZoom = maxZoom;
            return this;
        }

        public Builder setMinZoom(int minZoom) {
            this.minZoom = minZoom;
            return this;
        }

        public Builder setCurrentZoom(int currentZoom) {
            this.currentZoom = currentZoom;
            return this;
        }

        public Builder setFocusMode(int focusMode) {
            this.focusMode = focusMode;
            return this;
        }

        public Builder setExposureMode(int exposureMode) {
            this.exposureMode = exposureMode;
            return this;
        }

        public PhotoCapabilities build() {
            return new PhotoCapabilities(maxIso, minIso, currentIso, maxHeight, minHeight,
                    currentHeight, maxWidth, minWidth, currentWidth, maxZoom, minZoom, currentZoom,
                    focusMode, exposureMode);
        }
    }
}
