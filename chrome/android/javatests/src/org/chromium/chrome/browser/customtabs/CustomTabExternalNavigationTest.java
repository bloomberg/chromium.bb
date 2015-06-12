// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.os.Bundle;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.customtabs.CustomTab.CustomTabNavigationDelegate;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.externalnav.ExternalNavigationParams;

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

    private static final String TEST_URL = "about:blank";
    private ExternalNavigationHandler mUrlHandler;
    private CustomTabNavigationDelegate mNavigationDelegate;

    @Override
    public void startMainActivity() throws InterruptedException {
        super.startMainActivity();
        startCustomTabActivityWithIntent(createMinimalCustomTabIntent(TEST_URL));
        Tab tab = getActivity().getActivityTab();
        assertTrue("A custom tab is not present in the activity.", tab instanceof CustomTab);
        CustomTab customTab = (CustomTab) tab;
        mUrlHandler = customTab.getExternalNavigationHandler();
        mNavigationDelegate = customTab.getExternalNavigationDelegate();
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
