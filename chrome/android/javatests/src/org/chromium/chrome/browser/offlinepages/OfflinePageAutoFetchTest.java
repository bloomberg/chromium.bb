// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.util.WebServer;
import org.chromium.net.test.util.WebServer.HTTPRequest;
import org.chromium.ui.base.PageTransition;

import java.io.IOException;
import java.io.OutputStream;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for auto-fetch-on-net-error-page. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=AutoFetchOnNetErrorPage", "disable-features=NewNetErrorPageUI"})
public class OfflinePageAutoFetchTest {
    private static final String TAG = "AutoFetchTest";
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private Profile mProfile;
    private OfflinePageBridge mOfflinePageBridge;
    private CallbackHelper mPageAddedHelper = new CallbackHelper();
    private OfflinePageItem mAddedPage;
    private WebServer mWebServer;

    private void startWebServer() throws Exception {
        Assert.assertTrue(mWebServer == null);
        mWebServer = new WebServer(0, false);
        mWebServer.setRequestHandler((HTTPRequest request, OutputStream stream) -> {
            try {
                WebServer.writeResponse(
                        stream, WebServer.STATUS_OK, "<html>Hello World!</html>".getBytes());
            } catch (IOException e) {
            }
        });
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mProfile = activityTab().getProfile();
            mOfflinePageBridge = OfflinePageBridge.getForProfile(mProfile);

            if (!NetworkChangeNotifier.isInitialized()) {
                NetworkChangeNotifier.init();
            }
            NetworkChangeNotifier.forceConnectivityState(false);

            OfflinePageBridge.getForProfile(mProfile).addObserver(
                    new OfflinePageBridge.OfflinePageModelObserver() {
                        @Override
                        public void offlinePageAdded(OfflinePageItem addedPage) {
                            mAddedPage = addedPage;
                            mPageAddedHelper.notifyCalled();
                        }
                    });
        });
    }

    @After
    public void tearDown() throws Exception {
        OfflineTestUtil.clearIntercepts();
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"OfflineAutoFetch"})
    public void testAutoFetchOnDinoPage() throws Exception {
        startWebServer();
        final String testUrl = mWebServer.getBaseUrl();

        // Make |testUrl| return an offline error and attempt to load the page.
        // This should trigger an auto-fetch request.
        OfflineTestUtil.interceptWithOfflineError(testUrl);
        attemptLoadPage(testUrl);
        waitForRequestCount(1);

        // Navigate away from the page, so that the auto-fetch request is allowed to complete,
        // and go back online.
        attemptLoadPage(UrlConstants.ABOUT_URL);
        // The tab no longer has the requested URL active, so the in-progress notification should
        // appear.
        waitForInProgressNotification();
        OfflineTestUtil.clearIntercepts();
        ThreadUtils.runOnUiThreadBlocking(() -> NetworkChangeNotifier.forceConnectivityState(true));
        OfflineTestUtil.startRequestCoordinatorProcessing();

        // Wait for the background request to complete.
        waitForPageAdded();
        Assert.assertTrue(mAddedPage != null);
        // TODO(crbug.com/883486): Check for complete notification.
    }

    @Test
    @MediumTest
    @Feature({"OfflineAutoFetch"})
    public void testAutoFetchCancelOnLoad() throws Exception {
        startWebServer();
        final String testUrl = mWebServer.getBaseUrl();
        // Make |testUrl| return an offline error and attempt to load the page.
        // This should trigger an auto-fetch request.
        OfflineTestUtil.interceptWithOfflineError(testUrl);
        attemptLoadPage(testUrl);
        waitForRequestCount(1);

        // Allow loading the page and try again.
        OfflineTestUtil.clearIntercepts();
        OfflineTestUtil.startRequestCoordinatorProcessing();
        mActivityTestRule.loadUrl(testUrl);

        // |testUrl| should successfully load and the auto-fetch request should be removed.
        waitForRequestCount(0);
        Assert.assertEquals(0, mPageAddedHelper.getCallCount());
    }

    @Test
    @MediumTest
    @Feature({"OfflineAutoFetch"})
    public void testAutoFetchRequestRetainedOnOtherTabClosed() throws Exception {
        startWebServer();
        final String testUrl = mWebServer.getBaseUrl();
        // Make |testUrl| return an offline error and attempt to load the page.
        // This should trigger an auto-fetch request.
        OfflineTestUtil.interceptWithOfflineError(testUrl);
        attemptLoadPage(testUrl);
        waitForRequestCount(1);

        // Attempt to load the same URL in a new tab, and then close the tab.
        // This should not create a new request.
        Tab newTab = attemptLoadPageInNewTab(testUrl);
        closeTab(newTab);
        Assert.assertEquals(1, OfflineTestUtil.getRequestsInQueue().length);

        // The original request should remain. Allow the request to complete.
        closeTab(activityTab());
        // TODO(crbug.com/883486): Check for in-progress notification.
        OfflineTestUtil.clearIntercepts();
        ThreadUtils.runOnUiThreadBlocking(() -> NetworkChangeNotifier.forceConnectivityState(true));
        OfflineTestUtil.startRequestCoordinatorProcessing();
        waitForPageAdded();
    }

    // TODO(crbug.com/883486): Handling tab close is not yet implemented, so this test fails.
    @Test
    @DisabledTest
    // @MediumTest
    // @Feature({"OfflineAutoFetch"})
    public void testAutoFetchNotifyOnTabClose() throws Exception {
        final String testUrl = "http://www.offline.com";
        // Make |testUrl| return an offline error and attempt to load the page.
        // This should trigger an auto-fetch request.
        OfflineTestUtil.interceptWithOfflineError(testUrl);
        attemptLoadPage(testUrl);
        waitForRequestCount(1);

        closeTab(activityTab());

        waitForInProgressNotification();
    }

    // TODO(crbug.com/883486): Have a test that results in two simultaneous Auto-fetch requests.

    private void waitForRequestCount(int requestCount) {
        CriteriaHelper.pollInstrumentationThread(
                () -> OfflineTestUtil.getRequestsInQueue().length == requestCount);
    }

    // Wait until at least one auto-fetch request has shown an in-progress notification.
    private void waitForInProgressNotification() throws Exception {
        // TODO(crbug.com/883486): This just verifies that the request coordinator state is updated.
        // Once the in-progress notification is added, we should verify we attempted to show the
        // notification.
        CriteriaHelper.pollInstrumentationThread(() -> {
            for (SavePageRequest request : OfflineTestUtil.getRequestsInQueue()) {
                if (request.getAutoFetchNotificationState() == 1) {
                    return true;
                }
            }
            return false;
        });
    }

    private void waitForPageAdded() throws Exception {
        mPageAddedHelper.waitForCallback(0, 1, 10000, TimeUnit.MILLISECONDS);
    }

    private Tab activityTab() {
        return mActivityTestRule.getActivity().getActivityTab();
    }

    // Attempt to load a page on the active tab. Does not assert that the page is loaded
    // successfully.
    private void attemptLoadPage(String url) {
        Tab tab = activityTab();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            tab.loadUrl(
                    new LoadUrlParams(url, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR));
        });
    }

    // Attempts to create a new tab and load |url| in it.
    private Tab attemptLoadPageInNewTab(String url) throws Exception {
        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = ThreadUtils.runOnUiThreadBlocking(
                () -> activity.getTabCreator(false).launchUrl(url, TabLaunchType.FROM_LINK));
        ChromeTabUtils.waitForInteractable(tab);
        return tab;
    }

    private boolean isErrorPage(final Tab tab) {
        final AtomicReference<Boolean> result = new AtomicReference<Boolean>(false);
        ThreadUtils.runOnUiThreadBlocking(() -> result.set(tab.isShowingErrorPage()));
        return result.get();
    }

    private void closeTab(Tab tab) {
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();

        // Attempt to close the tab, which will delay closing until the undo timeout goes away.
        ThreadUtils.runOnUiThreadBlocking(
                () -> { TabModelUtils.closeTabById(model, tab.getId(), true); });
    }
}
