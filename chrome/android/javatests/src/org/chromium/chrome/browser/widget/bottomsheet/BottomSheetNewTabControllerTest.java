// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_PHONE;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.ChromeHomeNewTabPageBase;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.widget.FadingBackgroundView;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the NTP UI displayed when Chrome Home is enabled. TODO(twellington): Remove remaining
 * tests for ChromeHomeNewTabPage after it's completely removed.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({BottomSheetTestRule.ENABLE_CHROME_HOME,
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        BottomSheetTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
@Restriction(RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public class BottomSheetNewTabControllerTest {
    private FadingBackgroundView mFadingBackgroundView;
    private TestTabModelObserver mTabModelObserver;
    private BottomSheet mBottomSheet;
    private ChromeTabbedActivity mActivity;

    /** An observer used to detect changes in the tab model. */
    private static class TestTabModelObserver extends EmptyTabModelObserver {
        private final CallbackHelper mDidCloseTabCallbackHelper = new CallbackHelper();

        @Override
        public void didCloseTab(int tabId, boolean incognito) {
            mDidCloseTabCallbackHelper.notifyCalled();
        }
    }

    @Rule
    public BottomSheetTestRule mBottomSheetTestRule = new BottomSheetTestRule();

    @Before
    public void setUp() throws Exception {
        mBottomSheetTestRule.startMainActivityOnBlankPage();
        mBottomSheet = mBottomSheetTestRule.getBottomSheet();
        mTabModelObserver = new TestTabModelObserver();
        mActivity = mBottomSheetTestRule.getActivity();
        mActivity.getTabModelSelector().getModel(false).addObserver(mTabModelObserver);
        mActivity.getTabModelSelector().getModel(true).addObserver(mTabModelObserver);
        mFadingBackgroundView = mActivity.getFadingBackgroundView();
        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
    }

    @Test
    @SmallTest
    public void testNTPOverTabSwitcher_Normal() {
        assertEquals(1, mActivity.getTabModelSelector().getTotalTabCount());
        assertFalse("Overview mode should not be showing",
                mActivity.getLayoutManager().overviewVisible());
        assertFalse(mActivity.getTabModelSelector().isIncognitoSelected());

        // Select "New tab" from the menu.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), mActivity, R.id.new_tab_menu_id);

        // The sheet should be opened at half height over the tab switcher and the tab count should
        // remain unchanged.
        validateState(false, BottomSheet.SHEET_STATE_HALF);
        assertEquals(1, mActivity.getTabModelSelector().getTotalTabCount());
        assertTrue(
                "Overview mode should be showing", mActivity.getLayoutManager().overviewVisible());

        // Load a URL in the bottom sheet.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.loadUrlInNewTab(new LoadUrlParams("about:blank"));
            }
        });

        // The sheet should now be closed and a regular tab should have been created
        validateState(false, BottomSheet.SHEET_STATE_PEEK);
        assertEquals(2, mActivity.getTabModelSelector().getTotalTabCount());
        assertFalse("Overview mode not should be showing",
                mActivity.getLayoutManager().overviewVisible());
        assertFalse(mActivity.getTabModelSelector().isIncognitoSelected());
    }

    @Test
    @SmallTest
    public void testNTPOverTabSwitcher_Incognito() {
        assertEquals(1, mActivity.getTabModelSelector().getTotalTabCount());
        assertFalse("Overview mode should not be showing",
                mActivity.getLayoutManager().overviewVisible());
        assertFalse(mActivity.getTabModelSelector().isIncognitoSelected());

        // Select "New incognito tab" from the menu.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivity, R.id.new_incognito_tab_menu_id);
        // The sheet should be opened at full height over the tab switcher and the tab count should
        // remain unchanged. The incognito model should now be selected.
        validateState(false, BottomSheet.SHEET_STATE_FULL);
        assertEquals(1, mActivity.getTabModelSelector().getTotalTabCount());
        assertTrue(
                "Overview mode should be showing", mActivity.getLayoutManager().overviewVisible());
        assertTrue(mActivity.getTabModelSelector().isIncognitoSelected());

        // Check that the normal model is selected after the sheet is closed without a URL being
        // loaded.
        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
        assertFalse(mActivity.getTabModelSelector().isIncognitoSelected());

        // Select "New incognito tab" from the menu and load a URL.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivity, R.id.new_incognito_tab_menu_id);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.loadUrlInNewTab(new LoadUrlParams("about:blank"));
            }
        });

        // The sheet should now be closed and an incognito tab should have been created
        validateState(false, BottomSheet.SHEET_STATE_PEEK);
        assertEquals(2, mActivity.getTabModelSelector().getTotalTabCount());
        assertTrue(mActivity.getActivityTab().isIncognito());
        assertFalse("Overview mode not should be showing",
                mActivity.getLayoutManager().overviewVisible());
        assertTrue(mActivity.getTabModelSelector().isIncognitoSelected());
    }

    @Test
    @SmallTest
    public void testNTPOverTabSwitcher_NoTabs() throws InterruptedException {
        // Close all tabs.
        ChromeTabUtils.closeAllTabs(InstrumentationRegistry.getInstrumentation(), mActivity);
        assertEquals(0, mActivity.getTabModelSelector().getTotalTabCount());

        // Select "New tab" from the menu.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), mActivity, R.id.new_tab_menu_id);

        // The sheet should be opened at full height.
        validateState(false, BottomSheet.SHEET_STATE_FULL);
        assertEquals(0, mActivity.getTabModelSelector().getTotalTabCount());
    }

    @Test
    @SmallTest
    public void testCloseNTP()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        // Create a new tab.
        createNewBlankTab(false);
        loadChromeHomeNewTab();

        // Close the new tab.
        closeNewTab();
        assertEquals(1, mActivity.getTabModelSelector().getTotalTabCount());
        assertFalse("Overview mode should not be showing",
                mActivity.getLayoutManager().overviewVisible());
    }

    @Test
    @SmallTest
    public void testCloseNTP_Incognito()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        // Create new incognito NTP.
        createNewBlankTab(true);
        loadChromeHomeNewTab();

        // Close the new tab.
        closeNewTab();
        assertEquals(1, mActivity.getTabModelSelector().getTotalTabCount());
        assertFalse("Overview mode should not be showing",
                mActivity.getLayoutManager().overviewVisible());
    }

    private void loadChromeHomeNewTab() throws InterruptedException {
        final Tab tab = mActivity.getActivityTab();
        ChromeTabUtils.loadUrlOnUiThread(tab, UrlConstants.NTP_URL);
        ChromeTabUtils.waitForTabPageLoaded(tab, UrlConstants.NTP_URL);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void createNewBlankTab(final boolean incognito) {
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivity, incognito ? R.id.new_incognito_tab_menu_id : R.id.new_tab_menu_id);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.loadUrl(new LoadUrlParams("about:blank"), incognito);
                mActivity.getLayoutManager().getActiveLayout().finishAnimationsForTests();
            }
        });
    }

    private void closeNewTab() throws InterruptedException, TimeoutException {
        int currentCallCount = mTabModelObserver.mDidCloseTabCallbackHelper.getCallCount();
        Tab tab = mActivity.getActivityTab();
        final ChromeHomeNewTabPageBase newTabPage = (ChromeHomeNewTabPageBase) tab.getNativePage();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                newTabPage.getCloseButtonForTests().callOnClick();
                mActivity.getLayoutManager().getActiveLayout().finishAnimationsForTests();
            }
        });

        mTabModelObserver.mDidCloseTabCallbackHelper.waitForCallback(currentCallCount, 1);

        validateState(false, BottomSheet.SHEET_STATE_PEEK);
    }

    private void validateState(boolean chromeHomeTabPageSelected, int expectedEndState) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.endAnimationsForTests();
            }
        });

        assertEquals("Sheet state incorrect", expectedEndState, mBottomSheet.getSheetState());

        if (chromeHomeTabPageSelected) {
            assertFalse(mFadingBackgroundView.isEnabled());
            assertEquals(0f, mFadingBackgroundView.getAlpha(), 0);
        } else {
            assertTrue(mFadingBackgroundView.isEnabled());
        }
    }
}
