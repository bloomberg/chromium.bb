// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests the behavior of the bottom sheet when used with the back button.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        BottomSheetTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public class BottomSheetBackBehaviorTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/simple.html";

    private BottomSheet mBottomSheet;
    private ChromeTabbedActivity mActivity;
    private LayoutManagerChrome mLayoutManager;

    @Rule
    public BottomSheetTestRule mBottomSheetTestRule = new BottomSheetTestRule();

    @Before
    public void setUp() throws Exception {
        mBottomSheetTestRule.startMainActivityOnBlankPage();
        mBottomSheet = mBottomSheetTestRule.getBottomSheet();
        mActivity = mBottomSheetTestRule.getActivity();
        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
        mLayoutManager = mActivity.getLayoutManager();
    }

    @Test
    @SmallTest
    public void testBackButton_sheetOpen() {
        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_HALF, false);

        pressBackButton();
        endBottomSheetAnimations();

        assertFalse("Overview mode should not be showing.", mLayoutManager.overviewVisible());
        assertEquals("The bottom sheet should be peeking.", BottomSheet.SHEET_STATE_PEEK,
                mBottomSheet.getSheetState());
    }

    @Test
    @SmallTest
    public void testBackButton_tabSwitcher() throws InterruptedException, TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mLayoutManager.showOverview(false);
            }
        });

        assertTrue("Overview mode should be showing.", mLayoutManager.overviewVisible());

        pressBackButton();
        endBottomSheetAnimations();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mLayoutManager.getActiveLayout().finishAnimationsForTests();
            }
        });

        assertFalse("Overview mode should not be showing.", mLayoutManager.overviewVisible());
        assertEquals("The bottom sheet should be peeking.", BottomSheet.SHEET_STATE_PEEK,
                mBottomSheet.getSheetState());
    }

    @Test
    @SmallTest
    public void testBackButton_backFromInternalNewTab()
            throws ExecutionException, InterruptedException, TimeoutException {
        Tab tab = launchNewTabFromChrome("about:blank");

        assertEquals("Tab should be on about:blank.", "about:blank", tab.getUrl());

        // Back button press should open the bottom sheet since backward navigation is not
        // possible.
        pressBackButton();
        endBottomSheetAnimations();

        assertEquals("The bottom sheet should be at half height.", BottomSheet.SHEET_STATE_HALF,
                mBottomSheet.getSheetState());
        assertFalse("Overview mode should not be showing.", mLayoutManager.overviewVisible());

        // Final back press should close the sheet and chrome.
        pressBackButton();
        endBottomSheetAnimations();

        assertEquals("The bottom sheet should be peeking.", BottomSheet.SHEET_STATE_PEEK,
                mBottomSheet.getSheetState());
        assertFalse("Chrome should no longer have focus.", mActivity.hasWindowFocus());
    }

    @Test
    @SmallTest
    public void testBackButton_backFromInternalNewTab_sheetOpen()
            throws ExecutionException, InterruptedException, TimeoutException {
        Tab tab = launchNewTabFromChrome("about:blank");

        assertEquals("Tab should be on about:blank.", "about:blank", tab.getUrl());

        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_HALF, false);

        // Back button should close the sheet but not send Chrome to the background.
        pressBackButton();
        endBottomSheetAnimations();

        assertEquals("The bottom sheet should be peeking.", BottomSheet.SHEET_STATE_PEEK,
                mBottomSheet.getSheetState());
        assertTrue("Chrome should have focus.", mActivity.hasWindowFocus());

        // Back button press should open the bottom sheet since backward navigation is not
        // possible.
        pressBackButton();
        endBottomSheetAnimations();

        assertEquals("The bottom sheet should be at half height.", BottomSheet.SHEET_STATE_HALF,
                mBottomSheet.getSheetState());
        assertFalse("Overview mode should not be showing.", mLayoutManager.overviewVisible());

        // Final back press should close the sheet and chrome.
        pressBackButton();
        endBottomSheetAnimations();

        assertEquals("The bottom sheet should be peeking.", BottomSheet.SHEET_STATE_PEEK,
                mBottomSheet.getSheetState());
        assertFalse("Chrome should no longer have focus.", mActivity.hasWindowFocus());
    }

    @Test
    @SmallTest
    public void testBackButton_backButtonOpensSheetAndShowsToolbar()
            throws ExecutionException, InterruptedException, TimeoutException {
        final Tab tab = launchNewTabFromChrome("about:blank");

        assertEquals("Tab should be on about:blank.", "about:blank", tab.getUrl());

        assertEquals("The bottom sheet should be peeking.", BottomSheet.SHEET_STATE_PEEK,
                mBottomSheet.getSheetState());

        // This also waits for the controls to be hidden.
        hideBrowserControls(tab);

        // Back button press should open the bottom sheet since backward navigation is not
        // possible and show the toolbar.
        pressBackButton();
        endBottomSheetAnimations();

        waitForShownBrowserControls();
        assertEquals("The bottom sheet should be at half height.", BottomSheet.SHEET_STATE_HALF,
                mBottomSheet.getSheetState());
    }

    @Test
    @SmallTest
    public void testBackButton_backWithNavigation()
            throws ExecutionException, InterruptedException, TimeoutException {
        final Tab tab = mBottomSheet.getActiveTab();

        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());

        String testUrl = testServer.getURL(TEST_PAGE);
        ChromeTabUtils.loadUrlOnUiThread(tab, testUrl);
        ChromeTabUtils.waitForTabPageLoaded(tab, testUrl);

        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_HALF, false);

        // Back button should close the bottom sheet.
        pressBackButton();
        endBottomSheetAnimations();

        assertEquals("Tab should be on the test page.", testUrl, tab.getUrl());
        assertEquals("The bottom sheet should be peeking.", BottomSheet.SHEET_STATE_PEEK,
                mBottomSheet.getSheetState());
        assertFalse("Overview mode should not be showing.", mLayoutManager.overviewVisible());

        // Next back button press should navigate back.
        pressBackButton();
        endBottomSheetAnimations();
        ChromeTabUtils.waitForTabPageLoaded(tab, "about:blank");

        assertEquals("Tab should be on about:blank.", "about:blank", tab.getUrl());
        assertEquals("The bottom sheet should be peeking.", BottomSheet.SHEET_STATE_PEEK,
                mBottomSheet.getSheetState());
        assertFalse("Overview mode should not be showing.", mLayoutManager.overviewVisible());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/766350")
    public void testBackButton_backFromExternalNewTab()
            throws InterruptedException, TimeoutException {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        launchNewTabFromExternalApp(testServer.getURL(TEST_PAGE));

        // Back button should send Chrome to the background.
        pressBackButton();
        endBottomSheetAnimations();

        assertEquals("The bottom sheet should be peeking.", BottomSheet.SHEET_STATE_PEEK,
                mBottomSheet.getSheetState());
        assertFalse("Chrome should no longer have focus.", mActivity.hasWindowFocus());
    }

    /**
     * Launch a new tab from an "external" app.
     * @param url The URL to launch in the tab.
     */
    private void launchNewTabFromExternalApp(String url) throws InterruptedException {
        final Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, "externalApp");
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        intent.setData(Uri.parse(url));

        final Tab originalTab = mActivity.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivity.onNewIntent(intent);
            }
        });
        CriteriaHelper.pollUiThread(new Criteria("Failed to select different tab") {
            @Override
            public boolean isSatisfied() {
                return mActivity.getActivityTab() != originalTab;
            }
        });
        ChromeTabUtils.waitForTabPageLoaded(mActivity.getActivityTab(), url);
    }

    /**
     * Launch a new tab from Chrome internally.
     * @param url The URL to launch in the tab.
     */
    private Tab launchNewTabFromChrome(final String url) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
            @Override
            public Tab call() {
                return mActivity.getTabCreator(false).launchUrl(
                        url, TabLaunchType.FROM_CHROME_UI);
            }
        });
    }

    /**
     * Wait for the browser controls to be hidden.
     * @param tab The active tab.
     */
    private void hideBrowserControls(final Tab tab) throws ExecutionException {
        // Hide the browser controls.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.getActivity().getFullscreenManager().setHideBrowserControlsAndroidView(true);
            }
        });

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mBottomSheet.isToolbarAndroidViewHidden();
            }
        });
    }

    /**
     * Wait for the browser controls to be shown.
     */
    private void waitForShownBrowserControls() throws ExecutionException {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !mBottomSheet.isToolbarAndroidViewHidden();
            }
        });
    }

    /** Notify the activity that the hardware back button was pressed. */
    private void pressBackButton() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivity.onBackPressed();
            }
        });
    }

    /** End bottom sheet animations. */
    private void endBottomSheetAnimations() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.endAnimations();
            }
        });
    }
}
