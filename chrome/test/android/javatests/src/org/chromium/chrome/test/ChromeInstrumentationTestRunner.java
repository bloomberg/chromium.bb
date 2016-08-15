// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import org.chromium.base.test.BaseChromiumInstrumentationTestRunner;
import org.chromium.base.test.BaseTestResult;
import org.chromium.base.test.util.DisableIfSkipCheck;
import org.chromium.base.test.util.RestrictionSkipCheck;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.test.util.ChromeDisableIf;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.policy.test.annotations.Policies;
import org.chromium.ui.base.DeviceFormFactor;

/**
 *  An Instrumentation test runner that optionally spawns a test HTTP server.
 *  The server's root directory is the device's external storage directory.
 *
 *  TODO(jbudorick): remove uses of deprecated org.apache.* crbug.com/488192
 */
@SuppressWarnings("deprecation")
public class ChromeInstrumentationTestRunner extends BaseChromiumInstrumentationTestRunner {

    private static final String TAG = "ChromeInstrumentationTestRunner";

    @Override
    public void onCreate(Bundle arguments) {
        super.onCreate(arguments);
    }

    @Override
    protected void addTestHooks(BaseTestResult result) {
        super.addTestHooks(result);
        result.addSkipCheck(new ChromeRestrictionSkipCheck(getTargetContext()));
        result.addSkipCheck(new ChromeDisableIfSkipCheck(getTargetContext()));

        result.addPreTestHook(Policies.getRegistrationHook());
    }

    private class ChromeRestrictionSkipCheck extends RestrictionSkipCheck {

        public ChromeRestrictionSkipCheck(Context targetContext) {
            super(targetContext);
        }

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
            if (TextUtils.equals(restriction,
                    ChromeRestriction.RESTRICTION_TYPE_OFFICIAL_BUILD)
                    && (!ChromeVersionInfo.isOfficialBuild())) {
                return true;
            }
            return false;
        }
    }

    private class ChromeDisableIfSkipCheck extends DisableIfSkipCheck {

        private final Context mTargetContext;

        public ChromeDisableIfSkipCheck(Context targetContext) {
            mTargetContext = targetContext;
        }

        @Override
        protected boolean deviceTypeApplies(String type) {
            if (TextUtils.equals(type, ChromeDisableIf.PHONE)
                    && !DeviceFormFactor.isTablet(getTargetContext())) {
                return true;
            }
            if (TextUtils.equals(type, ChromeDisableIf.TABLET)
                    && DeviceFormFactor.isTablet(getTargetContext())) {
                return true;
            }
            if (TextUtils.equals(type, ChromeDisableIf.LARGETABLET)
                    && DeviceFormFactor.isLargeTablet(getTargetContext())) {
                return true;
            }
            return false;
        }
    }
}
