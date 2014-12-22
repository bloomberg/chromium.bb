// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.graphics.Color;

/**
 * Utilities for brand color related properties.
 */
public class BrandColorUtils {
    private static final float CONTRAST_LIGHT_TEXT_THRESHOLD = 3f;
    private static final float LIGHTNESS_OPAQUE_BOX_THRESHOLD = 0.82f;

    /** Percentage to darken the brand color by when setting the status bar color. */
    private static final float DARKEN_COLOR_FRACTION = 0.6f;

    /**
     * Computes the lightness value in HSL standard for the given color.
     */
    private static float getLightnessForColor(int color) {
        int red = Color.red(color);
        int green = Color.green(color);
        int blue = Color.blue(color);
        int largest = Math.max(red, Math.max(green, blue));
        int smallest = Math.min(red, Math.min(green, blue));
        int average = (largest + smallest) / 2;
        return average / 255.0f;
    }

    /** Calculates the contrast between the given color and white, using the algorithm provided by
     * the WCAG v2 in http://www.w3.org/TR/WCAG20/#contrast-ratiodef.
     */
    private static float getContrastForColor(int color) {
        float bgR = Color.red(color) / 255f;
        float bgG = Color.green(color) / 255f;
        float bgB = Color.blue(color) / 255f;
        bgR = (bgR < 0.03928f) ? bgR / 12.92f : (float) Math.pow((bgR + 0.055f) / 1.055f, 2.4f);
        bgG = (bgG < 0.03928f) ? bgG / 12.92f : (float) Math.pow((bgG + 0.055f) / 1.055f, 2.4f);
        bgB = (bgB < 0.03928f) ? bgB / 12.92f : (float) Math.pow((bgB + 0.055f) / 1.055f, 2.4f);
        float bgL = 0.2126f * bgR + 0.7152f * bgG + 0.0722f * bgB;
        return Math.abs((1.05f) / (bgL + 0.05f));
    }

    /**
     * Darkens the given color to use on the status bar.
     * @param color Brand color of the website.
     * @return Color that should be used for Android status bar.
     */
    public static int computeStatusBarColor(int color) {
        float[] hsv = new float[3];
        Color.colorToHSV(color, hsv);
        hsv[2] *= DARKEN_COLOR_FRACTION;
        return Color.HSVToColor(hsv);
    }

    /**
     * Check which version of the drawables should be used depending on the given primary color.
     * @param color The primary color value we are querying for.
     * @return Whether the light colored text and drawable set should be used.
     */
    public static boolean shouldUseLightDrawablesForToolbar(int color) {
        return getContrastForColor(color) >= CONTRAST_LIGHT_TEXT_THRESHOLD;
    }

    /**
     * Check which version of the textbox background should be used depending on the given
     * primary color.
     * @param color The primary color value we are querying for.
     * @return Whether the transparent version of the background should be used.
     */
    public static boolean shouldUseOpaqueTextboxBackground(int color) {
        return getLightnessForColor(color) > LIGHTNESS_OPAQUE_BOX_THRESHOLD;
    }
}
