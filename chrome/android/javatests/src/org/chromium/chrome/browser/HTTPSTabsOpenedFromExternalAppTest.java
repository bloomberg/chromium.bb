// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.chrome.browser.TabsOpenedFromExternalAppTest.HTTP_REFERRER;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.blink_public.web.WebReferrerPolicy;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

/**
 * Test the behavior of tabs when opening an HTTPS URL from an external app.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})

public class HTTPSTabsOpenedFromExternalAppTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Tests that an http:// referrer is not stripped in case of https:// navigation with
     * default Policy.
     * @throws InterruptedException
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testReferrerPolicyHttpReferrerHttpsNavigationsPolicyDefault()
            throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        try {
            String url = mTestServer.getURL("/chrome/test/data/android/about.html");
            TabsOpenedFromExternalAppTest.launchAndVerifyReferrerWithPolicy(url, mActivityTestRule,
                    WebReferrerPolicy.WEB_REFERRER_POLICY_DEFAULT, HTTP_REFERRER, HTTP_REFERRER);
        } finally {
            if (mTestServer != null) mTestServer.stopAndDestroyServer();
        }
    }
}
