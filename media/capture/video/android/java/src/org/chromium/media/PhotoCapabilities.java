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
    public final int stepIso;
    public final int maxHeight;
    public final int minHeight;
    public final int currentHeight;
    public final int stepHeight;
    public final int maxWidth;
    public final int minWidth;
    public final int currentWidth;
    public final int stepWidth;
    public final int maxZoom;
    public final int minZoom;
    public final int currentZoom;
    public final int stepZoom;
    public final int focusMode;
    public final int exposureMode;
    public final int maxExposureCompensation;
    public final int minExposureCompensation;
    public final int currentExposureCompensation;
    public final int stepExposureCompensation;
    public final int whiteBalanceMode;
    public final int fillLightMode;
    public final boolean redEyeReduction;
    public final int maxColorTemperature;
    public final int minColorTemperature;
    public final int currentColorTemperature;
    public final int stepColorTemperature;

    PhotoCapabilities(int maxIso, int minIso, int currentIso, int stepIso, int maxHeight,
            int minHeight, int currentHeight, int stepHeight, int maxWidth, int minWidth,
            int currentWidth, int stepWidth, int maxZoom, int minZoom, int currentZoom,
            int stepZoom, int focusMode, int exposureMode, int maxExposureCompensation,
            int minExposureCompensation, int currentExposureCompensation,
            int stepExposureCompensation, int whiteBalanceMode, int fillLightMode,
            boolean redEyeReduction, int maxColorTemperature, int minColorTemperature,
            int currentColorTemperature, int stepColorTemperature) {
        this.maxIso = maxIso;
        this.minIso = minIso;
        this.currentIso = currentIso;
        this.stepIso = stepIso;
        this.maxHeight = maxHeight;
        this.minHeight = minHeight;
        this.currentHeight = currentHeight;
        this.stepHeight = stepHeight;
        this.maxWidth = maxWidth;
        this.minWidth = minWidth;
        this.currentWidth = currentWidth;
        this.stepWidth = stepWidth;
        this.maxZoom = maxZoom;
        this.minZoom = minZoom;
        this.currentZoom = currentZoom;
        this.stepZoom = stepZoom;
        this.focusMode = focusMode;
        this.exposureMode = exposureMode;
        this.maxExposureCompensation = maxExposureCompensation;
        this.minExposureCompensation = minExposureCompensation;
        this.currentExposureCompensation = currentExposureCompensation;
        this.stepExposureCompensation = stepExposureCompensation;
        this.whiteBalanceMode = whiteBalanceMode;
        this.fillLightMode = fillLightMode;
        this.redEyeReduction = redEyeReduction;
        this.maxColorTemperature = maxColorTemperature;
        this.minColorTemperature = minColorTemperature;
        this.currentColorTemperature = currentColorTemperature;
        this.stepColorTemperature = stepColorTemperature;
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
    public int getStepIso() {
        return stepIso;
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
    public int getStepHeight() {
        return stepHeight;
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
    public int getStepWidth() {
        return stepWidth;
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
    public int getStepZoom() {
        return stepZoom;
    }

    @CalledByNative
    public int getFocusMode() {
        return focusMode;
    }

    @CalledByNative
    public int getExposureMode() {
        return exposureMode;
    }

    @CalledByNative
    public int getMinExposureCompensation() {
        return minExposureCompensation;
    }

    @CalledByNative
    public int getMaxExposureCompensation() {
        return maxExposureCompensation;
    }

    @CalledByNative
    public int getCurrentExposureCompensation() {
        return currentExposureCompensation;
    }

    @CalledByNative
    public int getStepExposureCompensation() {
        return stepExposureCompensation;
    }

    @CalledByNative
    public int getWhiteBalanceMode() {
        return whiteBalanceMode;
    }

    @CalledByNative
    public int getFillLightMode() {
        return fillLightMode;
    }

    @CalledByNative
    public boolean getRedEyeReduction() {
        return redEyeReduction;
    }

    @CalledByNative
    public int getMinColorTemperature() {
        return minColorTemperature;
    }

    @CalledByNative
    public int getMaxColorTemperature() {
        return maxColorTemperature;
    }

    @CalledByNative
    public int getCurrentColorTemperature() {
        return currentColorTemperature;
    }

    @CalledByNative
    public int getStepColorTemperature() {
        return stepColorTemperature;
    }

    public static class Builder {
        public int maxIso;
        public int minIso;
        public int currentIso;
        public int stepIso;
        public int maxHeight;
        public int minHeight;
        public int currentHeight;
        public int stepHeight;
        public int maxWidth;
        public int minWidth;
        public int currentWidth;
        public int stepWidth;
        public int maxZoom;
        public int minZoom;
        public int currentZoom;
        public int stepZoom;
        public int focusMode;
        public int exposureMode;
        public int maxExposureCompensation;
        public int minExposureCompensation;
        public int currentExposureCompensation;
        public int stepExposureCompensation;
        public int whiteBalanceMode;
        public int fillLightMode;
        public boolean redEyeReduction;
        public int maxColorTemperature;
        public int minColorTemperature;
        public int currentColorTemperature;
        public int stepColorTemperature;

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

        public Builder setStepIso(int stepIso) {
            this.stepIso = stepIso;
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

        public Builder setStepHeight(int stepHeight) {
            this.stepHeight = stepHeight;
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

        public Builder setStepWidth(int stepWidth) {
            this.stepWidth = stepWidth;
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

        public Builder setStepZoom(int stepZoom) {
            this.stepZoom = stepZoom;
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

        public Builder setMaxExposureCompensation(int maxExposureCompensation) {
            this.maxExposureCompensation = maxExposureCompensation;
            return this;
        }

        public Builder setMinExposureCompensation(int minExposureCompensation) {
            this.minExposureCompensation = minExposureCompensation;
            return this;
        }

        public Builder setCurrentExposureCompensation(int currentExposureCompensation) {
            this.currentExposureCompensation = currentExposureCompensation;
            return this;
        }

        public Builder setStepExposureCompensation(int stepExposureCompensation) {
            this.stepExposureCompensation = stepExposureCompensation;
            return this;
        }

        public Builder setWhiteBalanceMode(int whiteBalanceMode) {
            this.whiteBalanceMode = whiteBalanceMode;
            return this;
        }

        public Builder setFillLightMode(int fillLightMode) {
            this.fillLightMode = fillLightMode;
            return this;
        }

        public Builder setRedEyeReduction(boolean redEyeReduction) {
            this.redEyeReduction = redEyeReduction;
            return this;
        }

        public Builder setMaxColorTemperature(int maxColorTemperature) {
            this.maxColorTemperature = maxColorTemperature;
            return this;
        }

        public Builder setMinColorTemperature(int minColorTemperature) {
            this.minColorTemperature = minColorTemperature;
            return this;
        }

        public Builder setCurrentColorTemperature(int currentColorTemperature) {
            this.currentColorTemperature = currentColorTemperature;
            return this;
        }

        public Builder setStepColorTemperature(int stepColorTemperature) {
            this.stepColorTemperature = stepColorTemperature;
            return this;
        }

        public PhotoCapabilities build() {
            return new PhotoCapabilities(maxIso, minIso, currentIso, stepIso, maxHeight, minHeight,
                    currentHeight, stepHeight, maxWidth, minWidth, currentWidth, stepWidth, maxZoom,
                    minZoom, currentZoom, stepZoom, focusMode, exposureMode,
                    maxExposureCompensation, minExposureCompensation, currentExposureCompensation,
                    stepExposureCompensation, whiteBalanceMode, fillLightMode, redEyeReduction,
                    maxColorTemperature, minColorTemperature, currentColorTemperature,
                    stepColorTemperature);
        }
    }
}
