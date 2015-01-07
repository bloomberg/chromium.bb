// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.test.AndroidTestRunner;
import android.test.InstrumentationTestRunner;
import android.util.Log;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.chromium.base.test.util.MinAndroidSdkLevel;

/**
 *  An Instrumentation test runner that checks SDK level for tests with specific requirements.
 */
public class BaseInstrumentationTestRunner extends InstrumentationTestRunner {

    private static final String TAG = "BaseInstrumentationTestRunner";

    @Override
    protected AndroidTestRunner getAndroidTestRunner() {
        return new BaseAndroidTestRunner(getContext());
    }

    /**
     *  Skips tests that don't meet the requirements of the current device.
     */
    public class BaseAndroidTestRunner extends AndroidTestRunner {
        private final Context mContext;

        public BaseAndroidTestRunner(Context context) {
            mContext = context;
        }

        @Override
        public void setTest(Test test) {
            super.setTest(test);
            TestSuite revisedTestSuite = new TestSuite();
            for (TestCase testCase : this.getTestCases()) {
                Class<?> testClass = testCase.getClass();
                if (shouldSkip(testClass, testCase)) {
                    revisedTestSuite.addTest(new SkippedTest(testCase));
                    Bundle skipResult = new Bundle();
                    skipResult.putBoolean("test_skipped", true);
                    sendStatus(0, skipResult);
                } else {
                    revisedTestSuite.addTest(testCase);
                }
            }
            super.setTest(revisedTestSuite);
        }

        protected boolean shouldSkip(Class<?> testClass, TestCase testCase) {
            if (testClass.isAnnotationPresent(MinAndroidSdkLevel.class)) {
                MinAndroidSdkLevel v = testClass.getAnnotation(MinAndroidSdkLevel.class);
                if (Build.VERSION.SDK_INT < v.value()) {
                    Log.i(TAG, "Test " + testClass.getName() + "#" + testCase.getName()
                            + " is not enabled at SDK level " + Build.VERSION.SDK_INT
                            + ".");
                    return true;
                }
            }
            return false;
        }

        protected Context getContext() {
            return mContext;
        }
    }

    /**
     *  Replaces a TestCase that should be skipped.
     */
    public static class SkippedTest extends TestCase {

        public SkippedTest(TestCase skipped) {
            super(skipped.getClass().getName() + "#" + skipped.getName());
        }

        @Override
        protected void runTest() throws Throwable {
        }

        @Override
        public String toString() {
            return "SKIPPED " + super.toString();
        }
    }
}
