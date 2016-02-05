// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import junit.framework.TestCase;

import org.chromium.base.test.BaseInstrumentationTestRunner;
import org.chromium.base.test.BaseTestResult;
import org.chromium.base.test.util.SkipCheck;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.policy.test.annotations.Policies;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.reflect.Method;

/**
 *  An Instrumentation test runner that optionally spawns a test HTTP server.
 *  The server's root directory is the device's external storage directory.
 *
 *  TODO(jbudorick): remove uses of deprecated org.apache.* crbug.com/488192
 */
@SuppressWarnings("deprecation")
public class ChromeInstrumentationTestRunner extends BaseInstrumentationTestRunner {

    private static final String TAG = "ChromeInstrumentationTestRunner";

    @Override
    public void onCreate(Bundle arguments) {
        super.onCreate(arguments);
    }

    @Override
    protected void addTestHooks(BaseTestResult result) {
        super.addTestHooks(result);
        result.addSkipCheck(new DisableInTabbedModeSkipCheck());
        result.addSkipCheck(new ChromeRestrictionSkipCheck());

        result.addPreTestHook(Policies.getRegistrationHook());
    }

    private class ChromeRestrictionSkipCheck extends RestrictionSkipCheck {

        @Override
        protected boolean restrictionApplies(String restriction) {
            if (TextUtils.equals(restriction, ChromeRestriction.RESTRICTION_TYPE_PHONE)
                    && DeviceFormFactor.isTablet(getTargetContext())) {
                return true;
            }
            if (TextUtils.equals(restriction, ChromeRestriction.RESTRICTION_TYPE_TABLET)
                    && !DeviceFormFactor.isTablet(getTargetContext())) {
                return true;
            }
            if (TextUtils.equals(restriction,
                                 ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
                    && (ConnectionResult.SUCCESS != GoogleApiAvailability.getInstance()
                            .isGooglePlayServicesAvailable(getTargetContext()))) {
                return true;
            }
            return false;
        }
    }

    /**
     * Checks for tests that should only run in document mode.
     */
    private class DisableInTabbedModeSkipCheck extends SkipCheck {

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
