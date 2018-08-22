// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.download.DownloadActivity;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;

/** Unit tests for offline indicator interacting with chrome activity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.OFFLINE_INDICATOR})
// TODO(jianli): Add test for disabled feature.
public class OfflineIndicatorControllerTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";

    private boolean mIsConnected = true;

    @Before
    public void setUp() throws Exception {
        ConnectivityDetector.skipSystemCheckForTesting();
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            if (!NetworkChangeNotifier.isInitialized()) {
                NetworkChangeNotifier.init();
            }
            NetworkChangeNotifier.forceConnectivityState(true);
        });
    }

    @Test
    @MediumTest
    public void testShowOfflineIndicatorOnNTPWhenOffline() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL(TEST_PAGE);

        // Load new tab page.
        loadPage(UrlConstants.NTP_URL);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), true);
    }

    @Test
    @MediumTest
    public void testShowOfflineIndicatorOnRegularPageWhenOffline() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL(TEST_PAGE);

        // Load a page.
        loadPage(testUrl);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), true);
    }

    @Test
    @MediumTest
    public void testHideOfflineIndicatorWhenBackToOnline() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL(TEST_PAGE);

        // Load a page.
        loadPage(testUrl);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), true);

        // Reconnect the network.
        setNetworkConnectivity(true);

        // Offline indicator should go away.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), false);
    }

    @Test
    @MediumTest
    public void testDoNotShowOfflineIndicatorOnErrorPageWhenOffline() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL(TEST_PAGE);

        // Stop the server and also disconnect the network.
        testServer.shutdownAndWaitUntilComplete();
        setNetworkConnectivity(false);

        // Load an error page.
        loadPage(testUrl);

        // Reconnect the network.
        setNetworkConnectivity(true);

        // Offline indicator should not be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), false);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should not be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), false);
    }

    @Test
    @MediumTest
    public void testDoNotShowOfflineIndicatorOnDownloadsWhenOffline() throws Exception {
        if (mActivityTestRule.getActivity().isTablet()) return;

        DownloadActivity downloadActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), DownloadActivity.class,
                new MenuUtils.MenuActivityTrigger(InstrumentationRegistry.getInstrumentation(),
                        mActivityTestRule.getActivity(), R.id.downloads_menu_id));

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should not be shown.
        checkOfflineIndicatorVisibility(downloadActivity, false);
    }

    private void setNetworkConnectivity(boolean connected) {
        mIsConnected = connected;
        ThreadUtils.runOnUiThreadBlocking(
                () -> { NetworkChangeNotifier.forceConnectivityState(connected); });
    }

    private void loadPage(String pageUrl) throws Exception {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        mActivityTestRule.loadUrl(pageUrl);
        Assert.assertEquals(pageUrl, tab.getUrl());
        if (mIsConnected) {
            Assert.assertFalse(isErrorPage(tab));
        } else {
            Assert.assertTrue(isErrorPage(tab));
        }
    }

    private static void checkOfflineIndicatorVisibility(
            SnackbarManageable activity, boolean visible) {
        CriteriaHelper.pollUiThread(
                new Criteria(visible ? "Offline indicator not shown" : "Offline indicator shown") {
                    @Override
                    public boolean isSatisfied() {
                        return visible == activity.getSnackbarManager().isShowing();
                    }
                });
    }

    private static boolean isErrorPage(final Tab tab) {
        final boolean[] isShowingError = new boolean[1];
        ThreadUtils.runOnUiThreadBlocking(() -> { isShowingError[0] = tab.isShowingErrorPage(); });
        return isShowingError[0];
    }
}
