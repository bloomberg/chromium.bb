// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_PHONE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_TABLET;

import android.test.InstrumentationTestCase;
import android.text.TextUtils;

import org.chromium.base.SysUtils;
import org.chromium.base.test.util.Restriction;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.reflect.Method;

/**
 * A version of {@link InstrumentationTestCase} that checks the {@link Restriction} annotation
 * before running each test.
 */
public class RestrictedInstrumentationTestCase extends InstrumentationTestCase {
    /**
     * Whether a test method should run based on the Restriction annotation.  This will test the
     * current test method being run by checking {@link InstrumentationTestCase#getName()}.
     * @param testCase The instrumentationTestCase to check against.
     * @throws Exception
     */
    public static boolean shouldRunTest(InstrumentationTestCase testCase)
            throws Exception {
        Method method = testCase.getClass().getMethod(testCase.getName(), (Class[]) null);
        Restriction restrictions = method.getAnnotation(Restriction.class);
        if (restrictions != null) {
            for (String restriction : restrictions.value()) {
                if (TextUtils.equals(restriction, RESTRICTION_TYPE_PHONE)
                        && DeviceFormFactor.isTablet(
                                testCase.getInstrumentation().getTargetContext())) {
                    return false;
                }
                if (TextUtils.equals(restriction, RESTRICTION_TYPE_TABLET)
                        && !DeviceFormFactor.isTablet(
                                testCase.getInstrumentation().getTargetContext())) {
                    return false;
                }
                if (TextUtils.equals(restriction, RESTRICTION_TYPE_LOW_END_DEVICE)
                        && !SysUtils.isLowEndDevice()) {
                    return false;
                }
                if (TextUtils.equals(restriction, RESTRICTION_TYPE_NON_LOW_END_DEVICE)
                        && SysUtils.isLowEndDevice()) {
                    return false;
                }
            }
        }
        return true;
    }

    @Override
    public void runTest() throws Throwable {
        boolean shouldRun = true;
        try {
            shouldRun = shouldRunTest(this);
        } catch (Exception e) {
            // eat the exception here; super.runTest() will catch it again and handle it properly
        }

        if (shouldRun) super.runTest();
    }
}