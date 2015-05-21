// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.Context;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.List;

/**
 * Checks to see that Main is properly displaying InfoBars when an update is detected.
 */
public class OmahaUpdateInfoBarTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String HTML_FILENAME_1 =
            "chrome/test/data/android/omahaupdateinfobar/omaha_update_infobar_test_page_1.html";
    private static final String HTML_FILENAME_2 =
            "chrome/test/data/android/omahaupdateinfobar/omaha_update_infobar_test_page_2.html";

    private static final long MS_TIMEOUT = 2000;
    private static final long MS_INTERVAL = 500;

    public OmahaUpdateInfoBarTest() {
        super(ChromeActivity.class);
    }

    /** Reports versions that we want back to OmahaClient. */
    private static class MockVersionNumberGetter extends VersionNumberGetter {
        // Both of these strings must be of the format "#.#.#.#".
        private final String mCurrentVersion;
        private final String mLatestVersion;

        private boolean mAskedForCurrentVersion;
        private boolean mAskedForLatestVersion;

        public MockVersionNumberGetter(String currentVersion, String latestVersion) {
            mCurrentVersion = currentVersion;
            mLatestVersion = latestVersion;
        }

        @Override
        public String getCurrentlyUsedVersion(Context applicationContext) {
            assertNotNull("Never set the current version", mCurrentVersion);
            mAskedForCurrentVersion = true;
            return mCurrentVersion;
        }

        @Override
        public String getLatestKnownVersion(
                Context applicationContext, String prefPackage, String prefLatestVersion) {
            assertNotNull("Never set the latest version", mLatestVersion);
            mAskedForLatestVersion = true;
            return mLatestVersion;
        }

        public boolean askedForCurrentVersion() {
            return mAskedForCurrentVersion;
        }

        public boolean askedForLatestVersion() {
            return mAskedForLatestVersion;
        }
    }

    /** Reports a dummy market URL back to OmahaClient. */
    private static class MockMarketURLGetter extends MarketURLGetter {
        private final String mURL;

        MockMarketURLGetter(String url) {
            mURL = url;
        }

        @Override
        public String getMarketURL(
                Context applicationContext, String prefPackage, String prefMarketUrl) {
            return mURL;
        }
    }

    private MockVersionNumberGetter mMockVersionNumberGetter;
    private MockMarketURLGetter mMockMarketURLGetter;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        // This test explicitly tests for the infobar, so turn it on.
        OmahaClient.setEnableUpdateDetection(true);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        // Don't start Main until we've had a chance to override everything.
    }

    /**
     * Prepares Main before actually launching it.  This is required since we don't have all of the
     * info we need in setUp().
     * @param currentVersion Version to report as the current version of Chrome
     * @param latestVersion Version to report is available by Omaha
     * @param url URL to show.  null = NTP.
     */
    private void prepareAndStartMainActivity(String currentVersion, String latestVersion,
            String url) throws Exception {
        if (url == null) url = UrlConstants.NTP_URL;

        // Report fake versions back to Main when it asks.
        mMockVersionNumberGetter = new MockVersionNumberGetter(currentVersion, latestVersion);
        OmahaClient.setVersionNumberGetterForTests(mMockVersionNumberGetter);

        // Report a dummy URL to Omaha.
        mMockMarketURLGetter = new MockMarketURLGetter(
                "https://market.android.com/details?id=com.google.android.apps.chrome");
        OmahaClient.setMarketURLGetterForTests(mMockMarketURLGetter);

        // Start up main.
        startMainActivityWithURL(url);

        // Check to make sure that the version numbers get queried.
        assertTrue("Main didn't ask Omaha for version numbers.", versionNumbersQueried());
    }

    private boolean versionNumbersQueried() throws Exception {
        return CriteriaHelper.pollForCriteria(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mMockVersionNumberGetter.askedForCurrentVersion()
                                && mMockVersionNumberGetter.askedForLatestVersion();
                    }
                },
                MS_TIMEOUT, MS_INTERVAL);
    }

    /**
     * Checks to see if one of the InfoBars is a NavigateToURLInfoBar pointing to the market.
     */
    private boolean correctInfoBarExists() {
        // Check the current InfoBars for a NavigateToURLInfoBar.
        List<InfoBar> infobars = getInfoBars();
        if (infobars == null) {
            return false;
        }

        // Check if one of the InfoBars is pointing to the market URL.
        Context context = getInstrumentation().getTargetContext();
        String expectedURL = OmahaClient.getMarketURL(context);
        for (int i = 0; i < infobars.size(); ++i) {
            if (infobars.get(i) instanceof OmahaUpdateInfobar) {
                OmahaUpdateInfobar bar = (OmahaUpdateInfobar) infobars.get(i);
                if (expectedURL.equals(bar.getUrl())) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * Waits until the NavigateToURLInfoBar appears for the current tab.
     */
    private boolean correctInfoBarAdded() throws Exception {
        return CriteriaHelper.pollForCriteria(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return correctInfoBarExists();

                    }
                },
                MS_TIMEOUT, MS_INTERVAL);
    }

    /**
     * Waits until the NavigateToURLInfoBar goes away.
     */
    private boolean correctInfoBarRemoved() throws Exception {
        return CriteriaHelper.pollForCriteria(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return !correctInfoBarExists();

                    }
                },
                MS_TIMEOUT, MS_INTERVAL);
    }

    /**
     * Checks to see that the InfoBar is created when an update is detected.
     */
    private void checkInfobarAppearsAndIsDismissed(String currentVersion, String latestVersion,
                String url) throws Exception {
        prepareAndStartMainActivity(currentVersion, latestVersion, url);

        if (url == null) {
            // The InfoBar shouldn't be created yet because we're on the NTP.
            assertFalse("InfoBar shown on NTP.", correctInfoBarAdded());

            // Navigate somewhere else and then check for the InfoBar.
            loadUrl(UrlUtils.getIsolatedTestFileUrl(HTML_FILENAME_1));
            assertTrue("InfoBar failed to show after navigating from NTP.",
                    correctInfoBarAdded());
        } else {
            // The InfoBar be shown ASAP since we're not on the NTP.
            assertTrue("InfoBar failed to show.", correctInfoBarAdded());
        }

        // Make sure the InfoBar doesn't disappear immediately.
        assertFalse("InfoBar was removed too quickly.", correctInfoBarRemoved());

        // Make sure the InfoBar goes away once we navigate somewhere else on the same tab.
        loadUrl(UrlUtils.getIsolatedTestFileUrl(HTML_FILENAME_2));
        assertTrue("InfoBar is still showing", correctInfoBarRemoved());
    }

    /**
     * Makes sure that no InfoBar is shown at all, after the initial page load or after
     * moving away.
     */
    private void checkInfobarDoesNotAppear(String currentVersion, String latestVersion, String url)
            throws Exception {
        prepareAndStartMainActivity(currentVersion, latestVersion, url);
        assertFalse("Infobar is showing.", correctInfoBarAdded());
        loadUrl(UrlUtils.getIsolatedTestFileUrl(HTML_FILENAME_2));
        assertFalse("Infobar is now showing", correctInfoBarAdded());
    }

    @MediumTest
    @Feature({"Omaha"})
    public void testCurrentVersionIsOlderNTP() throws Exception {
        checkInfobarAppearsAndIsDismissed("0.0.0.0", "1.2.3.4", null);
    }

    @MediumTest
    @Feature({"Omaha"})
    public void testCurrentVersionIsOlderNotNTP() throws Exception {
        checkInfobarAppearsAndIsDismissed("0.0.0.0", "1.2.3.4",
                UrlUtils.getIsolatedTestFileUrl(HTML_FILENAME_1));
    }

    @MediumTest
    @Feature({"Omaha"})
    public void testCurrentVersionIsSameNTP() throws Exception {
        checkInfobarDoesNotAppear("1.2.3.4", "1.2.3.4", null);
    }

    @MediumTest
    @Feature({"Omaha"})
    public void testCurrentVersionIsSameNotNTP() throws Exception {
        checkInfobarDoesNotAppear("1.2.3.4", "1.2.3.4",
                UrlUtils.getIsolatedTestFileUrl(HTML_FILENAME_1));
    }

    @MediumTest
    @Feature({"Omaha"})
    public void testCurrentVersionIsNewerNTP() throws Exception {
        checkInfobarDoesNotAppear("27.0.1453.42", "26.0.1410.49", null);
    }

    @MediumTest
    @Feature({"Omaha"})
    public void testCurrentVersionIsNewerNotNTP() throws Exception {
        checkInfobarDoesNotAppear("27.0.1453.42", "26.0.1410.49",
                UrlUtils.getIsolatedTestFileUrl(HTML_FILENAME_1));
    }

    @MediumTest
    @Feature({"Omaha"})
    public void testNoVersionKnownNTP() throws Exception {
        checkInfobarDoesNotAppear("1.2.3.4", "0", null);
    }

    @MediumTest
    @Feature({"Omaha"})
    public void testNoVersionKnownNotNTP() throws Exception {
        checkInfobarDoesNotAppear("1.2.3.4", "", UrlUtils.getIsolatedTestFileUrl(HTML_FILENAME_1));
    }
}
