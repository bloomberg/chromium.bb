// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.test.AndroidTestRunner;
import android.util.Log;

import junit.framework.TestCase;
import junit.framework.TestResult;

import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.test.reporter.TestStatusListener;

import java.lang.reflect.Method;

/**
 * An Instrumentation test runner that checks SDK level for tests with specific requirements.
 */
public class ChromeStagingInstrumentationTestRunner extends ChromeInstrumentationTestRunner {

    public static final String TAG = "ChromeInternalInstrumentationTestRunner";

    @Override
    protected AndroidTestRunner getAndroidTestRunner() {
        AndroidTestRunner runner = new AndroidTestRunner() {
            @Override
            protected TestResult createTestResult() {
                SkippingTestResult r = new SkippingTestResult();
                r.addSkipCheck(new MinAndroidSdkLevelSkipCheck());
                r.addSkipCheck(new DisableInTabbedModeSkipCheck());
                return r;
            }
        };
        runner.addTestListener(new TestStatusListener(getContext()));
        return runner;
    }

    /**
     * Checks for tests that should only run in document mode.
     */
    public class DisableInTabbedModeSkipCheck implements SkipCheck {

        /**
         * If the test is running in tabbed mode, checks for
         * {@link org.chromium.chrome.test.util.DisableInTabbedMode}.
         *
         * @param testCase The test to check.
         * @return Whether the test is running in tabbed mode and has been marked as disabled in
         *      tabbed mode.
         */
        @Override
        public boolean shouldSkip(TestCase testCase) {
            Class<?> testClass = testCase.getClass();
            try {
                if (!FeatureUtilities.isDocumentMode(getContext())) {
                    Method testMethod = testClass.getMethod(testCase.getName());
                    if (testMethod.isAnnotationPresent(DisableInTabbedMode.class)
                            || testClass.isAnnotationPresent(DisableInTabbedMode.class)) {
                        Log.i(TAG, "Test " + testClass.getName() + "#" + testCase.getName()
                                + " is disabled in non-document mode.");
                        return true;
                    }
                }
            } catch (NoSuchMethodException e) {
                Log.e(TAG, "Couldn't find test method: " + e.toString());
            }

            return false;
        }
    }
}

