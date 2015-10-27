// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.os.Bundle;
import android.test.FlakyTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory.CustomTabNavigationDelegate;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.externalnav.ExternalNavigationParams;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation test for external navigation handling in a {@link CustomTab}.
 */
public class CustomTabExternalNavigationTest extends CustomTabActivityTestBase {

    /**
     * A dummy activity that claims to handle "customtab://customtabtest".
     */
    public static class DummyActivityForSpecialScheme extends Activity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            finish();
        }
    }

    /**
     * A dummy activity that claims to handle "http://customtabtest.com".
     */
    public static class DummyActivityForHttp extends Activity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            finish();
        }
    }

    private static final String TEST_URL = TestHttpServerClient.getUrl(
            "chrome/test/data/android/google.html");
    private ExternalNavigationHandler mUrlHandler;
    private CustomTabNavigationDelegate mNavigationDelegate;

    @Override
    public void startMainActivity() throws InterruptedException {
        super.startMainActivity();
        startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(), TEST_URL, null));
        Tab tab = getActivity().getActivityTab();
        TabDelegateFactory delegateFactory = tab.getDelegateFactoryForTest();
        assert delegateFactory instanceof CustomTabDelegateFactory;
        mUrlHandler = ((CustomTabDelegateFactory) delegateFactory).getExternalNavigationHandler();
        mNavigationDelegate = ((CustomTabDelegateFactory) delegateFactory)
                .getExternalNavigationDelegate();
    }

    /**
     * For urls with special schemes and hosts, and there is exactly one activity having a matching
     * intent filter, the framework will make that activity the default handler of the special url.
     * This test tests whether chrome is able to start the default external handler.
     */
    @SmallTest
    public void testExternalActivityStartedForDefaultUrl() {
        final String testUrl = "customtab://customtabtest/intent";
        ExternalNavigationParams params = new ExternalNavigationParams.Builder(testUrl, false)
                .build();
        OverrideUrlLoadingResult result = mUrlHandler.shouldOverrideUrlLoading(params);
        assertEquals(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT, result);
        assertTrue("A dummy activity should have been started to handle the special url.",
                mNavigationDelegate.hasExternalActivityStarted());
    }

    /**
     * When loading a normal http url that chrome is able to handle, an intent picker should never
     * be shown, even if other activities such as {@link DummyActivityForHttp} claim to handle it.
     *
     * crbug.com/519613
     * @SmallTest
     */
    @FlakyTest
    public void testIntentPickerNotShownForNormalUrl() {
        final String testUrl = "http://customtabtest.com";
        ExternalNavigationParams params = new ExternalNavigationParams.Builder(testUrl, false)
                .build();
        OverrideUrlLoadingResult result = mUrlHandler.shouldOverrideUrlLoading(params);
        assertEquals(OverrideUrlLoadingResult.NO_OVERRIDE, result);
        assertFalse("External activities should not be started to handle the url",
                mNavigationDelegate.hasExternalActivityStarted());
    }

    /**
     * Tests whether a new tab can be created from a {@link CustomTab}.
     *
     * crbug.com/519613
     * @SmallTest
     */
    @FlakyTest
    public void testNotCreateNewTab() throws InterruptedException, TimeoutException {
        final String testUrl = TestHttpServerClient.getUrl("chrome/test/data/android/google.html");
        final TabModelSelector tabSelector = getActivity().getTabModelSelector();

        final CallbackHelper loadUrlHelper = new CallbackHelper();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                tabSelector.getCurrentTab().addObserver(new EmptyTabObserver() {
                    @Override
                    public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                        loadUrlHelper.notifyCalled();
                    }
                });
                tabSelector.openNewTab(new LoadUrlParams(testUrl), TabLaunchType.FROM_LINK, null,
                        false);
            }
        });

        loadUrlHelper.waitForCallback(0, 1);
        assertTrue("A new tab should not have been created.",
                ApplicationStatus.getLastTrackedFocusedActivity() == getActivity());
        assertEquals(testUrl, getActivity().getActivityTab().getUrl());
    }
}