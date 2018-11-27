// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import static org.chromium.chrome.browser.customtabs.CustomTabsDynamicModuleTestUtils.FAKE_MODULE_COMPONENT_NAME;

import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.customtabs.dynamicmodule.ModuleEntryPoint;
import org.chromium.chrome.browser.customtabs.dynamicmodule.ModuleLoader;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests for {@link ModuleLoader}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@SmallTest
public class CustomTabsDynamicModuleLoaderTest {
    @Before
    public void setUp() throws Exception {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
    }

    @Rule
    public Features.JUnitProcessor mProcessor = new Features.JUnitProcessor();

    /**
     * Test fake dynamic module is correctly loaded.
     */
    @Test
    @SmallTest
    public void testModuleLoading() throws TimeoutException, InterruptedException {
        ModuleLoader moduleLoader = new ModuleLoader(FAKE_MODULE_COMPONENT_NAME);
        moduleLoader.loadModule();

        CallbackHelper onLoaded = new CallbackHelper();
        moduleLoader.addCallbackAndIncrementUseCount(
                result -> {
                    assertNotNull(result);
                    onLoaded.notifyCalled();
                });

        onLoaded.waitForCallback();
    }

    /**
     * Test the ModuleLoader correctly update module usage counter.
     */
    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_MODULE, ChromeFeatureList.CCT_MODULE_CACHE})
    public void testModuleUseCounter() throws TimeoutException, InterruptedException {
        final int callbacksNumber = 3;
        ModuleLoader moduleLoader = new ModuleLoader(FAKE_MODULE_COMPONENT_NAME);

        CallbackHelper onLoaded = new CallbackHelper();

        // Test we correctly unregister callbacks which were never notified.
        List<Callback<ModuleEntryPoint>> unusedCallbacks = new ArrayList<>();
        for (int i = 0; i < callbacksNumber; i++) {
            unusedCallbacks.add(result -> {});
            moduleLoader.addCallbackAndIncrementUseCount(unusedCallbacks.get(i));
        }
        // module has not been loaded, therefore there is no usage
        assertEquals(0, moduleLoader.getModuleUseCount());

        // unregister all callbacks so they should not increment module usage
        for (int i = 0; i < callbacksNumber; i++) {
            moduleLoader.removeCallbackAndDecrementUseCount(unusedCallbacks.get(i));
        }

        assertEquals(0, moduleLoader.getModuleUseCount());

        moduleLoader.loadModule();

        List<Callback<ModuleEntryPoint>> callbacks = new ArrayList<>();
        // register callbacks and wait for the notification when module is loaded
        for (int i = 0; i < callbacksNumber; i++) {
            callbacks.add(result -> onLoaded.notifyCalled());
            moduleLoader.addCallbackAndIncrementUseCount(callbacks.get(i));
        }
        onLoaded.waitForCallback(0, callbacksNumber);

        assertEquals(callbacksNumber, moduleLoader.getModuleUseCount());

        // unregister already notified callbacks
        for (int i = 0; i < callbacksNumber; i++) {
            moduleLoader.removeCallbackAndDecrementUseCount(callbacks.get(i));
        }
        assertEquals(0, moduleLoader.getModuleUseCount());
    }
}