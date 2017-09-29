// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ssl;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.util.Base64;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.parameter.CommandLineParameter;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.X509Util;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.net.test.util.CertTestUtil;

import java.util.concurrent.Callable;

/** Tests for the Captive portal interstitial. */
@RunWith(ChromeJUnit4ClassRunner.class)
@MediumTest
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
@CommandLineParameter({"", "enable-features=" + ChromeFeatureList.CAPTIVE_PORTAL_CERTIFICATE_LIST})
public class CaptivePortalTest {
    private static final String CAPTIVE_PORTAL_INTERSTITIAL_TITLE_PREFIX = "Connect to";
    private static final int INTERSTITIAL_TITLE_UPDATE_TIMEOUT_SECONDS = 5;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityFromLauncher();
        mServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_MISMATCHED_NAME);

        CaptivePortalHelper.setOSReportsCaptivePortalForTesting(false);
        CaptivePortalHelper.setCaptivePortalCertificateForTesting("sha256/test");
    }

    @After
    public void tearDown() throws Exception {
        mServer.stopAndDestroyServer();
    }

    private void waitForInterstitial(final WebContents webContents, final boolean shouldBeShown) {
        CriteriaHelper.pollUiThread(Criteria.equals(shouldBeShown, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return webContents.isShowingInterstitialPage();
            }
        }));
    }

    /** Navigate the tab to an interstitial with a name mismatch error and check if this
    /*  results in a captive portal interstitial.
     */
    private void navigateAndCheckCaptivePortalInterstitial() throws Exception {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        ChromeTabUtils.loadUrlOnUiThread(
                tab, mServer.getURL("/chrome/test/data/android/navigate/simple.html"));
        waitForInterstitial(tab.getWebContents(), true);
        Assert.assertTrue(tab.isShowingInterstitialPage());

        new TabTitleObserver(tab, CAPTIVE_PORTAL_INTERSTITIAL_TITLE_PREFIX) {
            @Override
            protected boolean doesTitleMatch(String expectedTitle, String actualTitle) {
                return actualTitle.indexOf(expectedTitle) == 0;
            }
        }
                .waitForTitleUpdate(INTERSTITIAL_TITLE_UPDATE_TIMEOUT_SECONDS);

        Assert.assertEquals(0, tab.getTitle().indexOf(CAPTIVE_PORTAL_INTERSTITIAL_TITLE_PREFIX));
    }

    @Test
    public void testCaptivePortalCertificateListFeature() throws Exception {
        // Add the SPKI of the root cert to captive portal certificate list.
        byte[] rootCertSPKI = CertTestUtil.getPublicKeySha256(X509Util.createCertificateFromBytes(
                CertTestUtil.pemToDer(mServer.getRootCertPemPath())));
        Assert.assertTrue(rootCertSPKI != null);
        CaptivePortalHelper.setCaptivePortalCertificateForTesting(
                "sha256/" + Base64.encodeToString(rootCertSPKI, Base64.NO_WRAP));

        navigateAndCheckCaptivePortalInterstitial();
    }

    @Test
    public void testOSReportsCaptivePortal() throws Exception {
        CaptivePortalHelper.setOSReportsCaptivePortalForTesting(true);
        navigateAndCheckCaptivePortalInterstitial();
    }
}
