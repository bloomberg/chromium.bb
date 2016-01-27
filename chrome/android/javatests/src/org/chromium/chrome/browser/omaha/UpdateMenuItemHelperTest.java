// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.Context;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;


/**
 * Tests for the UpdateMenuItemHelper.
 */
@CommandLineFlags.Add("enable_update_menu_item")
public class UpdateMenuItemHelperTest extends ChromeTabbedActivityTestBase {

    private static final long MS_TIMEOUT = 2000;
    private static final long MS_INTERVAL = 500;

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

        // This test explicitly tests for the menu item, so turn it on.
        OmahaClient.setEnableUpdateDetection(true);
    }

    @Override
    public void startMainActivity() {
        // Don't start Main until we've had a chance to override everything.
    }

    /**
     * Prepares Main before actually launching it.  This is required since we don't have all of the
     * info we need in setUp().
     * @param currentVersion Version to report as the current version of Chrome
     * @param latestVersion Version to report is available by Omaha
     */
    private void prepareAndStartMainActivity(String currentVersion, String latestVersion)
            throws Exception {
        // Report fake versions back to Main when it asks.
        mMockVersionNumberGetter = new MockVersionNumberGetter(currentVersion, latestVersion);
        OmahaClient.setVersionNumberGetterForTests(mMockVersionNumberGetter);

        // Report a dummy URL to Omaha.
        mMockMarketURLGetter = new MockMarketURLGetter(
                "https://play.google.com/store/apps/details?id=com.android.chrome");
        OmahaClient.setMarketURLGetterForTests(mMockMarketURLGetter);

        // Start up main.
        startMainActivityWithURL(UrlConstants.NTP_URL);

        // Check to make sure that the version numbers get queried.
        versionNumbersQueried();
    }

    private void versionNumbersQueried() throws Exception {
        CriteriaHelper.pollForCriteria(
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
     * Checks that the menu item is shown when a new version is available.
     */
    private void checkUpdateMenuItemIsShowing(String currentVersion, String latestVersion)
            throws Exception {
        prepareAndStartMainActivity(currentVersion, latestVersion);
        showAppMenuAndAssertMenuShown();
        assertTrue("Update menu item is not showing.",
                getActivity().getAppMenuHandler().getAppMenu().getMenu().findItem(
                        R.id.update_menu_id).isVisible());
    }

    /**
     * Checks that the menu item is not shown when a new version is not available.
     */
    private void checkUpdateMenuItemIsNotShowing(String currentVersion, String latestVersion)
            throws Exception {
        prepareAndStartMainActivity(currentVersion, latestVersion);
        showAppMenuAndAssertMenuShown();
        assertFalse("Update menu item is showing.",
                getActivity().getAppMenuHandler().getAppMenu().getMenu().findItem(
                        R.id.update_menu_id).isVisible());
    }

    @MediumTest
    @Feature({"Omaha"})
    public void testCurrentVersionIsOlder() throws Exception {
        checkUpdateMenuItemIsShowing("0.0.0.0", "1.2.3.4");
    }

    @MediumTest
    @Feature({"Omaha"})
    public void testCurrentVersionIsSame() throws Exception {
        checkUpdateMenuItemIsNotShowing("1.2.3.4", "1.2.3.4");
    }

    @MediumTest
    @Feature({"Omaha"})
    public void testCurrentVersionIsNewer() throws Exception {
        checkUpdateMenuItemIsNotShowing("27.0.1453.42", "26.0.1410.49");
    }

    @MediumTest
    @Feature({"Omaha"})
    public void testNoVersionKnown() throws Exception {
        checkUpdateMenuItemIsNotShowing("1.2.3.4", "0");
    }

    @MediumTest
    @Feature({"Omaha"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    public void testMenuItemNotShownInOverview() throws Exception {
        checkUpdateMenuItemIsShowing("0.0.0.0", "1.2.3.4");

        // checkUpdateMenuItemIsShowing() opens the menu; hide it and assert it's dismissed.
        hideAppMenuAndAssertMenuShown();

        // Ensure not shown in tab switcher app menu.
        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                getActivity().getLayoutManager(), true, false);
        View tabSwitcherButton = getActivity().findViewById(R.id.tab_switcher_button);
        assertNotNull("'tab_switcher_button' view is not found", tabSwitcherButton);
        singleClickView(tabSwitcherButton);
        overviewModeWatcher.waitForBehavior();
        showAppMenuAndAssertMenuShown();
        assertFalse("Update menu item is showing.",
                getActivity().getAppMenuHandler().getAppMenu().getMenu().findItem(
                        R.id.update_menu_id).isVisible());
    }

    private void showAppMenuAndAssertMenuShown() throws InterruptedException {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity().getAppMenuHandler().showAppMenu(null, false);
            }
        });
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return  getActivity().getAppMenuHandler().isAppMenuShowing();
            }
        });
    }

    private void hideAppMenuAndAssertMenuShown() throws InterruptedException {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity().getAppMenuHandler().hideAppMenu();
            }
        });
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return  !getActivity().getAppMenuHandler().isAppMenuShowing();
            }
        });
    }
}

