// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Build;
import android.os.Bundle;
import android.test.AndroidTestRunner;
import android.test.InstrumentationTestRunner;
import android.text.TextUtils;

import junit.framework.TestCase;
import junit.framework.TestResult;

import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.multidex.ChromiumMultiDex;
import org.chromium.base.test.BaseTestResult.SkipCheck;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.test.reporter.TestStatusListener;

import java.lang.reflect.Method;

// TODO(jbudorick): Add support for on-device handling of timeouts.
/**
 *  An Instrumentation test runner that checks SDK level for tests with specific requirements.
 */
public class BaseInstrumentationTestRunner extends InstrumentationTestRunner {
    private static final String TAG = "base_test";

    @Override
    public void onCreate(Bundle arguments) {
        ChromiumMultiDex.install(getTargetContext());
        super.onCreate(arguments);
    }

    @Override
    protected AndroidTestRunner getAndroidTestRunner() {
        AndroidTestRunner runner = new AndroidTestRunner() {
            @Override
            protected TestResult createTestResult() {
                BaseTestResult r = new BaseTestResult(BaseInstrumentationTestRunner.this);
                addTestHooks(r);
                return r;
            }
        };
        runner.addTestListener(new TestStatusListener(getContext()));
        return runner;
    }

    /**
     * Override this method to register hooks and checks to be run for each test. Make sure to call
     * the base implementation if you do so.
     *
     * @see BaseTestResult#addSkipCheck(BaseTestResult.SkipCheck)
     * @see BaseTestResult#addPreTestHook(BaseTestResult.PreTestHook)
     */
    protected void addTestHooks(BaseTestResult result) {
        result.addSkipCheck(new MinAndroidSdkLevelSkipCheck());
        result.addSkipCheck(new RestrictionSkipCheck());

        result.addPreTestHook(CommandLineFlags.getRegistrationHook());
    }

    /**
     * Checks if any restrictions exist and skip the test if it meets those restrictions.
     */
    public class RestrictionSkipCheck implements SkipCheck {
        @Override
        public boolean shouldSkip(TestCase testCase) {
            Method method;
            try {
                method = testCase.getClass().getMethod(testCase.getName(), (Class[]) null);
            } catch (NoSuchMethodException e) {
                Log.e(TAG, "Unable to find %s in %s", testCase.getName(),
                        testCase.getClass().getName(), e);
                return true;
            }
            Restriction restrictions = method.getAnnotation(Restriction.class);
            if (restrictions != null) {
                for (String restriction : restrictions.value()) {
                    if (restrictionApplies(restriction)) {
                        return true;
                    }
                }
            }
            return false;
        }

        protected boolean restrictionApplies(String restriction) {
            if (TextUtils.equals(restriction, Restriction.RESTRICTION_TYPE_LOW_END_DEVICE)
                    && !SysUtils.isLowEndDevice()) {
                return true;
            }
            if (TextUtils.equals(restriction, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
                    && SysUtils.isLowEndDevice()) {
                return true;
            }
            if (TextUtils.equals(restriction, Restriction.RESTRICTION_TYPE_INTERNET)
                    && !isNetworkAvailable()) {
                return true;
            }
            return false;
        }

        private boolean isNetworkAvailable() {
            final ConnectivityManager connectivityManager = (ConnectivityManager)
                    getTargetContext().getSystemService(Context.CONNECTIVITY_SERVICE);
            final NetworkInfo activeNetworkInfo = connectivityManager.getActiveNetworkInfo();
            return activeNetworkInfo != null && activeNetworkInfo.isConnected();
        }
    }

    /**
     * Checks the device's SDK level against any specified minimum requirement.
     */
    public static class MinAndroidSdkLevelSkipCheck implements SkipCheck {

        /**
         * If {@link MinAndroidSdkLevel} is present, checks its value
         * against the device's SDK level.
         *
         * @param testCase The test to check.
         * @return true if the device's SDK level is below the specified minimum.
         */
        @Override
        public boolean shouldSkip(TestCase testCase) {
            Class<?> testClass = testCase.getClass();
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
    }
}
