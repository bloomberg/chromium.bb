// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.EXTRA_MODULE_CLASS_NAME;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.EXTRA_MODULE_PACKAGE_NAME;
import static org.chromium.chrome.browser.customtabs.dynamicmodule.DynamicModuleNavigationEventObserver.PENDING_URL_KEY;
import static org.chromium.chrome.browser.customtabs.dynamicmodule.DynamicModuleNavigationEventObserver.URL_KEY;

import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.customtabs.CustomTabsCallback;
import android.support.test.InstrumentationRegistry;

import org.junit.Assert;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.AppHooksModule;
import org.chromium.chrome.browser.customtabs.dynamicmodule.BaseActivityDelegate;
import org.chromium.chrome.browser.customtabs.dynamicmodule.BaseModuleEntryPoint;
import org.chromium.chrome.browser.customtabs.dynamicmodule.IActivityDelegate;
import org.chromium.chrome.browser.customtabs.dynamicmodule.IActivityHost;
import org.chromium.chrome.browser.customtabs.dynamicmodule.IModuleHost;
import org.chromium.chrome.browser.customtabs.dynamicmodule.IObjectWrapper;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;

import java.util.concurrent.TimeoutException;

/**
 * Utility class that contains fake CCT dynamic module classes and convenience calls
 * related with CCT dynamic module testing.
 */
public class CustomTabsDynamicModuleTestUtils {
    /* package */ final static String FAKE_MODULE_PACKAGE_NAME =
            InstrumentationRegistry.getInstrumentation().getContext().getPackageName();
    /* package */ final static String FAKE_MODULE_CLASS_NAME = FakeCCTDynamicModule.class.getName();
    /* package */ final static ComponentName FAKE_MODULE_COMPONENT_NAME = new ComponentName(
            FAKE_MODULE_PACKAGE_NAME, FAKE_MODULE_CLASS_NAME);
    private static int sModuleVersion = 1;

    public static void setModuleVersion(int version) {
        sModuleVersion = version;
    }

    /**
     * To load a fake module in tests we need to bypass a check if package name of module
     * is Google-signed. This class overrides this check for testing.
     */
    /* package */ static class AppHooksModuleForTest extends AppHooksModule {
        @Override
        public ExternalAuthUtils provideExternalAuthUtils() {
            return new ExternalAuthUtils() {
                @Override
                public boolean isGoogleSigned(String packageName) {
                    return true;
                }
            };
        }
    }

    /**
     * This class is used to test CCT module loader.
     */
    public static class FakeCCTDynamicModule extends BaseModuleEntryPoint {
        @Override
        public void init(IModuleHost moduleHost) {
        }

        @Override
        public int getModuleVersion() {
            return sModuleVersion;
        }

        @Override
        public int getMinimumHostVersion() {
            return 0;
        }

        @Override
        public IActivityDelegate createActivityDelegate(IActivityHost activityHost) {
            return new FakeCCTActivityDelegate();
        }

        @Override
        public void onDestroy() {
        }
    }

    /**
     * Fake class to test CCT dynamic module.
     */
    public static class FakeCCTActivityDelegate extends BaseActivityDelegate {
        private final CallbackHelper mOnNavigationStarted = new CallbackHelper();
        private final CallbackHelper mOnNavigationFinished = new CallbackHelper();

        public FakeCCTActivityDelegate() {
        }

        @Override
        public void onCreate(Bundle savedInstanceState) {
        }

        @Override
        public void onResume() {
        }

        @Override
        public void onPostCreate(Bundle savedInstanceState) {
        }

        @Override
        public void onStart() {
        }

        @Override
        public void onStop() {
        }

        @Override
        public void onWindowFocusChanged(boolean hasFocus) {
        }

        @Override
        public void onSaveInstanceState(Bundle outState) {
        }

        @Override
        public void onRestoreInstanceState(Bundle savedInstanceState) {
        }

        @Override
        public void onPause() {
        }

        @Override
        public void onDestroy() {
        }

        @Override
        public boolean onBackPressed() {
            return false;
        }

        @Override
        public void onBackPressedAsync(IObjectWrapper notHandledRunnable) {
        }

        @Override
        public void onMessageChannelReady() {
        }

        @Override
        public void onPostMessage(String message) {
        }

        @Override
        public void onNavigationEvent(int navigationEvent, Bundle extras) {
            // Introduced in API version 4.
            if (sModuleVersion < 4) {
                Assert.fail("onNavigationEvent must not be used if module version less than 4");
            }

            if (navigationEvent == CustomTabsCallback.NAVIGATION_STARTED) {
                Assert.assertNotNull(extras.get(PENDING_URL_KEY));
                mOnNavigationStarted.notifyCalled();
            } else if (navigationEvent == CustomTabsCallback.NAVIGATION_FINISHED) {
                Assert.assertNotNull(extras.get(URL_KEY));
                mOnNavigationFinished.notifyCalled();
            }
        }

        /**
         * Waits for expected number of navigation events happen.
         */
        /* package */ void waitForNavigationEvent(int navigationEvent, int currentCallCount,
                int numberOfCallsToWaitFor) throws TimeoutException, InterruptedException {
            if (sModuleVersion < 4) return;
            if (navigationEvent == CustomTabsCallback.NAVIGATION_STARTED) {
                mOnNavigationStarted.waitForCallback(currentCallCount, numberOfCallsToWaitFor);
            } else if (navigationEvent == CustomTabsCallback.NAVIGATION_FINISHED) {
                mOnNavigationFinished.waitForCallback(currentCallCount, numberOfCallsToWaitFor);
            }
        }
    }

    /**
     * Creates the simplest intent that is sufficient to let {@link ChromeLauncherActivity} launch
     * the {@link CustomTabActivity} with fake dynamic module.
     *
     * @see #makeDynamicModuleIntent(ComponentName, String, String)
     */
    public static Intent makeDynamicModuleIntent(String url, @Nullable String managedUrlsRegex) {
        return makeDynamicModuleIntent(FAKE_MODULE_COMPONENT_NAME, url, managedUrlsRegex);
    }

    /**
     * Creates the simplest intent that is sufficient to let {@link ChromeLauncherActivity} launch
     * the {@link CustomTabActivity} with dynamic module.
     */
    public static Intent makeDynamicModuleIntent(
            ComponentName componentName, String url, @Nullable String managedUrlsRegex) {
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), url);
        intent.putExtra(EXTRA_MODULE_PACKAGE_NAME, componentName.getPackageName());
        intent.putExtra(EXTRA_MODULE_CLASS_NAME, componentName.getClassName());

        if (managedUrlsRegex != null) {
            intent.putExtra(CustomTabIntentDataProvider.EXTRA_MODULE_MANAGED_URLS_REGEX,
                    managedUrlsRegex);
        }
        return intent;
    }
}
