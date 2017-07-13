// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseChromiumInstrumentationTestRunner;
import org.chromium.base.test.BaseTestResult;
import org.chromium.base.test.util.DisableIfSkipCheck;
import org.chromium.base.test.util.RestrictionSkipCheck;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.vr_shell.VrClassesWrapper;
import org.chromium.chrome.browser.vr_shell.VrDaydreamApi;
import org.chromium.chrome.test.util.ChromeDisableIf;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.content.browser.test.ChildProcessAllocatorSettingsHook;
import org.chromium.policy.test.annotations.Policies;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.concurrent.Callable;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;

/**
 *  An Instrumentation test runner that optionally spawns a test HTTP server.
 *  The server's root directory is the device's external storage directory.
 */
public class ChromeInstrumentationTestRunner extends BaseChromiumInstrumentationTestRunner {
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
        result.addPreTestHook(new ChildProcessAllocatorSettingsHook());
    }

    static class ChromeRestrictionSkipCheck extends RestrictionSkipCheck {
        private VrDaydreamApi mDaydreamApi;
        private boolean mAttemptedToGetApi;

        public ChromeRestrictionSkipCheck(Context targetContext) {
            super(targetContext);
        }

        @SuppressWarnings("unchecked")
        private VrDaydreamApi getDaydreamApi() {
            if (!mAttemptedToGetApi) {
                mAttemptedToGetApi = true;
                try {
                    Class<? extends VrClassesWrapper> vrClassesBuilderClass =
                            (Class<? extends VrClassesWrapper>) Class.forName(
                                    "org.chromium.chrome.browser.vr_shell.VrClassesWrapperImpl");
                    Constructor<?> vrClassesBuilderConstructor =
                            vrClassesBuilderClass.getConstructor();
                    VrClassesWrapper vrClassesBuilder =
                            (VrClassesWrapper) vrClassesBuilderConstructor.newInstance();
                    mDaydreamApi = vrClassesBuilder.createVrDaydreamApi(getTargetContext());
                } catch (ClassNotFoundException | InstantiationException | IllegalAccessException
                        | IllegalArgumentException | InvocationTargetException
                        | NoSuchMethodException e) {
                    return null;
                }
            }
            return mDaydreamApi;
        }

        private boolean isDaydreamReady() {
            return getDaydreamApi() == null ? false :
                    getDaydreamApi().isDaydreamReadyDevice();
        }

        private boolean isDaydreamViewPaired() {
            if (getDaydreamApi() == null) {
                return false;
            }
            // isDaydreamCurrentViewer() creates a concrete instance of DaydreamApi,
            // which can only be done on the main thread
            FutureTask<Boolean> checker = new FutureTask<>(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return getDaydreamApi().isDaydreamCurrentViewer();
                }
            });
            ThreadUtils.runOnUiThreadBlocking(checker);
            try {
                return checker.get().booleanValue();
            } catch (CancellationException | InterruptedException | ExecutionException
                    | IllegalArgumentException e) {
                return false;
            }
        }

        private boolean isDonEnabled() {
            // We can't directly check whether the VR DON flow is enabled since
            // we don't have permission to read the VrCore settings file. Instead,
            // pass a flag.
            return CommandLine.getInstance().hasSwitch("don-enabled");
        }

        @Override
        protected boolean restrictionApplies(String restriction) {
            if (TextUtils.equals(restriction, ChromeRestriction.RESTRICTION_TYPE_PHONE)
                    && DeviceFormFactor.isTablet()) {
                return true;
            }
            if (TextUtils.equals(restriction, ChromeRestriction.RESTRICTION_TYPE_TABLET)
                    && !DeviceFormFactor.isTablet()) {
                return true;
            }
            if (TextUtils.equals(
                        restriction, ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES)
                    && (ConnectionResult.SUCCESS
                               != GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(
                                          getTargetContext()))) {
                return true;
            }
            if (TextUtils.equals(restriction,
                    ChromeRestriction.RESTRICTION_TYPE_OFFICIAL_BUILD)
                    && (!ChromeVersionInfo.isOfficialBuild())) {
                return true;
            }
            if (TextUtils.equals(restriction, ChromeRestriction.RESTRICTION_TYPE_DEVICE_DAYDREAM)
                    || TextUtils.equals(restriction,
                               ChromeRestriction.RESTRICTION_TYPE_DEVICE_NON_DAYDREAM)) {
                boolean isDaydream = isDaydreamReady();
                if (TextUtils.equals(
                            restriction, ChromeRestriction.RESTRICTION_TYPE_DEVICE_DAYDREAM)
                        && !isDaydream) {
                    return true;
                } else if (TextUtils.equals(restriction,
                                   ChromeRestriction.RESTRICTION_TYPE_DEVICE_NON_DAYDREAM)
                        && isDaydream) {
                    return true;
                }
            }
            if (TextUtils.equals(restriction, ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM)
                    || TextUtils.equals(restriction,
                               ChromeRestriction.RESTRICTION_TYPE_VIEWER_NON_DAYDREAM)) {
                boolean daydreamViewPaired = isDaydreamViewPaired();
                if (TextUtils.equals(
                            restriction, ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM)
                        && !daydreamViewPaired) {
                    return true;
                } else if (TextUtils.equals(restriction,
                                   ChromeRestriction.RESTRICTION_TYPE_VIEWER_NON_DAYDREAM)
                        && daydreamViewPaired) {
                    return true;
                }
            }
            if (TextUtils.equals(restriction, ChromeRestriction.RESTRICTION_TYPE_DON_ENABLED)) {
                return !isDonEnabled();
            }
            return false;
        }
    }

    static class ChromeDisableIfSkipCheck extends DisableIfSkipCheck {
        private final Context mTargetContext;

        public ChromeDisableIfSkipCheck(Context targetContext) {
            mTargetContext = targetContext;
        }

        @Override
        protected boolean deviceTypeApplies(String type) {
            if (TextUtils.equals(type, ChromeDisableIf.PHONE) && !DeviceFormFactor.isTablet()) {
                return true;
            }
            if (TextUtils.equals(type, ChromeDisableIf.TABLET) && DeviceFormFactor.isTablet()) {
                return true;
            }
            if (TextUtils.equals(type, ChromeDisableIf.LARGETABLET)
                    && DeviceFormFactor.isLargeTablet(mTargetContext)) {
                return true;
            }
            return false;
        }
    }
}
