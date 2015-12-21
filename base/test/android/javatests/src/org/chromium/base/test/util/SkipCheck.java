// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import junit.framework.TestCase;

import org.chromium.base.Log;

import java.lang.reflect.Method;

/**
 * Check whether a test case should be skipped.
 */
public abstract class SkipCheck {

    private static final String TAG = "base_test";

    /**
     *
     * Checks whether the given test case should be skipped.
     *
     * @param testCase The test case to check.
     * @return Whether the test case should be skipped.
     */
    public abstract boolean shouldSkip(TestCase testCase);

    protected static Method getTestMethod(TestCase testCase) {
        try {
            return testCase.getClass().getMethod(testCase.getName(), (Class[]) null);
        } catch (NoSuchMethodException e) {
            Log.e(TAG, "Unable to find %s in %s", testCase.getName(),
                    testCase.getClass().getName(), e);
            return null;
        }
    }
}

