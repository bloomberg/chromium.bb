// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.content.Context;
import android.content.Intent;
import android.support.customtabs.CustomTabsCallback;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.browser.AppHooksModule;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.dynamicmodule.CustomTabsDynamicModuleTestUtils.FakeCCTActivityDelegate;
import org.chromium.chrome.browser.customtabs.dynamicmodule.CustomTabsDynamicModuleTestUtils.IntentBuilder;
import org.chromium.chrome.browser.dependency_injection.ModuleFactoryOverrides;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for the CustomTabsDynamicModuleNavigationObserver.
 */
@RunWith(Parameterized.class)
public class CustomTabsDynamicModuleNavigationTest {
    @Rule
    public CustomTabActivityTestRule mActivityRule = new CustomTabActivityTestRule();

    /**
     * Test against different module versions i.e. before and after API was introduced.
     */
    @Parameterized.Parameters(name = "moduleVersion={0};")
    public static Object[] values() {
        return new Object[]{1, 4};
    }

    @Parameterized.Parameter
    public int moduleVersion;

    private String mTestPage;
    private String mTestPage2;
    private String mTestPage3;
    private EmbeddedTestServer mServer;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);

        ModuleFactoryOverrides.setOverride(AppHooksModule.Factory.class,
                CustomTabsDynamicModuleTestUtils.AppHooksModuleForTest::new);

        Context appContext = InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getApplicationContext();

        mServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mServer.getURL("/chrome/test/data/android/google.html");
        mTestPage2 = mServer.getURL("/chrome/test/data/android/simple.html");
        mTestPage3 = mServer.getURL("/chrome/test/data/android/about.html");
    }

    @After
    public void tearDown() {
        ModuleFactoryOverrides.clearOverrides();
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.CCT_MODULE)
    public void testModuleNavigationNotification()
            throws TimeoutException, InterruptedException {
        CustomTabsDynamicModuleTestUtils.setModuleVersion(moduleVersion);
        Intent intent = new IntentBuilder(mTestPage).build();

        mActivityRule.startCustomTabActivityWithIntent(intent);

        mActivityRule.loadUrlInTab(mTestPage2, PageTransition.LINK,
                getActivity().getActivityTab());
        mActivityRule.loadUrlInTab(mTestPage3, PageTransition.LINK,
                getActivity().getActivityTab());

        FakeCCTActivityDelegate activityDelegate =
                (FakeCCTActivityDelegate) getModuleCoordinator().getActivityDelegateForTesting();

        activityDelegate.waitForNavigationEvent(CustomTabsCallback.NAVIGATION_STARTED,
                0, 3);
        activityDelegate.waitForNavigationEvent(CustomTabsCallback.NAVIGATION_FINISHED,
                0, 3);
    }

    private CustomTabActivity getActivity() {
        return mActivityRule.getActivity();
    }

    private DynamicModuleCoordinator getModuleCoordinator() {
        return getActivity().getComponent().resolveDynamicModuleCoordinator();
    }
}