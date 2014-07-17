// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

/**
 * Contains various math utilities used throughout Chrome Mobile.
 */
public class MathUtils {

    private MathUtils() {}

    /**
     * Returns the passed in value if it resides within the specified range (inclusive).  If not,
     * it will return the closest boundary from the range.  The ordering of the boundary values does
     * not matter.
     *
     * @param value The value to be compared against the range.
     * @param a First boundary range value.
     * @param b Second boundary range value.
     * @return The passed in value if it is within the range, otherwise the closest boundary value.
     */
    public static int clamp(int value, int a, int b) {
        int min = (a > b) ? b : a;
        int max = (a > b) ? a : b;
        if (value < min) value = min;
        else if (value > max) value = max;
        return value;
    }

    /**
     * Returns the passed in value if it resides within the specified range (inclusive).  If not,
     * it will return the closest boundary from the range.  The ordering of the boundary values does
     * not matter.
     *
     * @param value The value to be compared against the range.
     * @param a First boundary range value.
     * @param b Second boundary range value.
     * @return The passed in value if it is within the range, otherwise the closest boundary value.
     */
    public static long clamp(long value, long a, long b) {
        long min = (a > b) ? b : a;
        long max = (a > b) ? a : b;
        if (value < min) value = min;
        else if (value > max) value = max;
        return value;
    }

    /**
     * Returns the passed in value if it resides within the specified range (inclusive).  If not,
     * it will return the closest boundary from the range.  The ordering of the boundary values does
     * not matter.
     *
     * @param value The value to be compared against the range.
     * @param a First boundary range value.
     * @param b Second boundary range value.
     * @return The passed in value if it is within the range, otherwise the closest boundary value.
     */
    public static float clamp(float value, float a, float b) {
        float min = (a > b) ? b : a;
        float max = (a > b) ? a : b;
        if (value < min) value = min;
        else if (value > max) value = max;
        return value;
    }

    /**
     * Computes a%b that is positive. Note that result of % operation is not always positive.
     * @return a%b >= 0 ? a%b : a%b + b
     */
    public static int positiveModulo(int a, int b) {
        int mod = a % b;
        return mod >= 0 ? mod : mod + b;
    }
}
