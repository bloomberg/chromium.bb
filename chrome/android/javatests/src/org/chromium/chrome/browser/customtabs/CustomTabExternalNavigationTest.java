// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.os.Bundle;
import android.os.Environment;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory.CustomTabNavigationDelegate;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.externalnav.ExternalNavigationParams;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Instrumentation test for external navigation handling of a Custom Tab.
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

    private static final String TEST_PATH = "/chrome/test/data/android/google.html";
    private CustomTabNavigationDelegate mNavigationDelegate;
    private EmbeddedTestServer mTestServer;
    private ExternalNavigationHandler mUrlHandler;

    public CustomTabExternalNavigationTest() {
        mSkipCheckHttpServer = true;
    }

    @Override
    public void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
        super.setUp();
    }

    @Override
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        super.startMainActivity();
        startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(), mTestServer.getURL(TEST_PATH), null));
        Tab tab = getActivity().getActivityTab();
        TabDelegateFactory delegateFactory = tab.getDelegateFactory();
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
     */
    @SmallTest
    public void testIntentPickerNotShownForNormalUrl() {
        final String testUrl = "http://customtabtest.com";
        ExternalNavigationParams params = new ExternalNavigationParams.Builder(testUrl, false)
                .build();
        OverrideUrlLoadingResult result = mUrlHandler.shouldOverrideUrlLoading(params);
        assertEquals(OverrideUrlLoadingResult.NO_OVERRIDE, result);
        assertFalse("External activities should not be started to handle the url",
                mNavigationDelegate.hasExternalActivityStarted());
    }
}
