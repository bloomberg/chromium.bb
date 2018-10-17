// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertNotNull;

import android.content.ComponentName;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.customtabs.dynamicmodule.BaseModuleEntryPoint;
import org.chromium.chrome.browser.customtabs.dynamicmodule.IActivityDelegate;
import org.chromium.chrome.browser.customtabs.dynamicmodule.IActivityHost;
import org.chromium.chrome.browser.customtabs.dynamicmodule.IModuleHost;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for CCT dynamic module functionality.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class CustomTabsDynamicModuleTest {
    private final CustomTabsConnection mConnection = CustomTabsConnection.getInstance();
    private final ComponentName mModuleComponentName = new ComponentName(
            InstrumentationRegistry.getInstrumentation().getContext().getPackageName(),
            FakeCCTDynamicModule.class.getName());

    @Before
    public void setUp() throws Exception {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
    }

    @Test
    @SmallTest
    public void testModuleLoading() throws TimeoutException, InterruptedException {
        CallbackHelper loadingWaiter = new CallbackHelper();
        mConnection.getModuleLoader(mModuleComponentName).loadModule(result -> {
            assertNotNull(result);
            loadingWaiter.notifyCalled();
        });
        loadingWaiter.waitForCallback();
    }

    /**
     * This class is used to test CCT module loader.
     */
    public static class FakeCCTDynamicModule extends BaseModuleEntryPoint {
        private static final int sVersion = 1;

        @Override
        public void init(IModuleHost moduleHost) {

        }

        @Override
        public int getModuleVersion() {
            return sVersion;
        }

        @Override
        public int getMinimumHostVersion() {
            return 0;
        }

        @Override
        public IActivityDelegate createActivityDelegate(IActivityHost activityHost) {
            return null;
        }

        @Override
        public void onDestroy() {
        }
    }
}