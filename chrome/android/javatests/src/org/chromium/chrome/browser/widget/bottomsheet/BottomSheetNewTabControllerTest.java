// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.ChromeHomeNewTabPageBase;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.widget.FadingBackgroundView;
import org.chromium.chrome.test.BottomSheetTestCaseBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the NTP UI displayed when Chrome Home is enabled.
 *
 * TODO(twellington): Remove remaining tests for ChromeHomeNewTabPage after it's completely removed.
 */
public class BottomSheetNewTabControllerTest extends BottomSheetTestCaseBase {
    private FadingBackgroundView mFadingBackgroundView;
    private TestTabModelObserver mTabModelObserver;

    /** An observer used to detect changes in the tab model. */
    private static class TestTabModelObserver extends EmptyTabModelObserver {
        private final CallbackHelper mDidCloseTabCallbackHelper = new CallbackHelper();

        @Override
        public void didCloseTab(int tabId, boolean incognito) {
            mDidCloseTabCallbackHelper.notifyCalled();
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mTabModelObserver = new TestTabModelObserver();
        getActivity().getTabModelSelector().getModel(false).addObserver(mTabModelObserver);
        getActivity().getTabModelSelector().getModel(true).addObserver(mTabModelObserver);

        mFadingBackgroundView = getActivity().getFadingBackgroundView();
    }

    @SmallTest
    public void testNTPOverTabSwitcher_Normal() throws InterruptedException, TimeoutException {
        assertEquals(1, getActivity().getTabModelSelector().getTotalTabCount());
        assertFalse("Overview mode should not be showing",
                getActivity().getLayoutManager().overviewVisible());
        assertFalse(getActivity().getTabModelSelector().isIncognitoSelected());

        // Select "New tab" from the menu.
        MenuUtils.invokeCustomMenuActionSync(
                getInstrumentation(), getActivity(), R.id.new_tab_menu_id);

        // The sheet should be opened at half height over the tab switcher and the tab count should
        // remain unchanged.
        validateState(false, BottomSheet.SHEET_STATE_HALF);
        assertEquals(1, getActivity().getTabModelSelector().getTotalTabCount());
        assertTrue("Overview mode should be showing",
                getActivity().getLayoutManager().overviewVisible());

        // Load a URL in the bottom sheet.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.loadUrlInNewTab(new LoadUrlParams("about:blank"));
            }
        });

        // The sheet should now be closed and a regular tab should have been created
        validateState(false, BottomSheet.SHEET_STATE_PEEK);
        assertEquals(2, getActivity().getTabModelSelector().getTotalTabCount());
        assertFalse("Overview mode not should be showing",
                getActivity().getLayoutManager().overviewVisible());
        assertFalse(getActivity().getTabModelSelector().isIncognitoSelected());
    }

    @SmallTest
    public void testNTPOverTabSwitcher_Incognito() throws InterruptedException, TimeoutException {
        assertEquals(1, getActivity().getTabModelSelector().getTotalTabCount());
        assertFalse("Overview mode should not be showing",
                getActivity().getLayoutManager().overviewVisible());
        assertFalse(getActivity().getTabModelSelector().isIncognitoSelected());

        // Select "New incognito tab" from the menu.
        MenuUtils.invokeCustomMenuActionSync(
                getInstrumentation(), getActivity(), R.id.new_incognito_tab_menu_id);
        // The sheet should be opened at half height over the tab switcher and the tab count should
        // remain unchanged. The incognito model should now be selected.
        validateState(false, BottomSheet.SHEET_STATE_HALF);
        assertEquals(1, getActivity().getTabModelSelector().getTotalTabCount());
        assertTrue("Overview mode should be showing",
                getActivity().getLayoutManager().overviewVisible());
        assertTrue(getActivity().getTabModelSelector().isIncognitoSelected());

        // Check that the normal model is selected after the sheet is closed without a URL being
        // loaded.
        setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
        assertFalse(getActivity().getTabModelSelector().isIncognitoSelected());

        // Select "New incognito tab" from the menu and load a URL.
        MenuUtils.invokeCustomMenuActionSync(
                getInstrumentation(), getActivity(), R.id.new_incognito_tab_menu_id);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.loadUrlInNewTab(new LoadUrlParams("about:blank"));
            }
        });

        // The sheet should now be closed and an incognito tab should have been created
        validateState(false, BottomSheet.SHEET_STATE_PEEK);
        assertEquals(2, getActivity().getTabModelSelector().getTotalTabCount());
        assertTrue(getActivity().getActivityTab().isIncognito());
        assertFalse("Overview mode not should be showing",
                getActivity().getLayoutManager().overviewVisible());
        assertTrue(getActivity().getTabModelSelector().isIncognitoSelected());
    }

    @SmallTest
    public void testNTPOverTabSwitcher_NoTabs() throws InterruptedException, TimeoutException {
        // Close all tabs.
        ChromeTabUtils.closeAllTabs(getInstrumentation(), getActivity());
        assertEquals(0, getActivity().getTabModelSelector().getTotalTabCount());

        // Select "New tab" from the menu.
        MenuUtils.invokeCustomMenuActionSync(
                getInstrumentation(), getActivity(), R.id.new_tab_menu_id);

        // The sheet should be opened at full height.
        validateState(false, BottomSheet.SHEET_STATE_FULL);
        assertEquals(0, getActivity().getTabModelSelector().getTotalTabCount());
    }

    @SmallTest
    @DisabledTest(message = "crbug.com/726032")
    public void testCloseNTP()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        // Create a new tab.
        createNewBlankTab(false);
        loadChromeHomeNewTab();

        // Close the new tab.
        closeNewTab();
        assertEquals(1, getActivity().getTabModelSelector().getTotalTabCount());
        assertFalse("Overview mode should not be showing",
                getActivity().getLayoutManager().overviewVisible());
    }

    @SmallTest
    @DisabledTest(message = "crbug.com/726032")
    public void testCloseNTP_Incognito()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        // Create new incognito NTP.
        createNewBlankTab(true);
        loadChromeHomeNewTab();

        // Close the new tab.
        closeNewTab();
        assertEquals(1, getActivity().getTabModelSelector().getTotalTabCount());
        assertFalse("Overview mode should not be showing",
                getActivity().getLayoutManager().overviewVisible());
    }

    private void loadChromeHomeNewTab() throws InterruptedException {
        final Tab tab = getActivity().getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(tab, new Runnable() {
            @Override
            public void run() {
                ChromeTabUtils.loadUrlOnUiThread(tab, UrlConstants.NTP_URL);
            }
        });
        getInstrumentation().waitForIdleSync();
    }

    private void createNewBlankTab(final boolean incognito) throws InterruptedException {
        MenuUtils.invokeCustomMenuActionSync(getInstrumentation(), getActivity(),
                incognito ? R.id.new_incognito_tab_menu_id : R.id.new_tab_menu_id);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.loadUrl(new LoadUrlParams("about:blank"), incognito);
            }
        });
    }

    private void closeNewTab() throws InterruptedException, TimeoutException {
        int currentCallCount = mTabModelObserver.mDidCloseTabCallbackHelper.getCallCount();
        Tab tab = getActivity().getActivityTab();
        final ChromeHomeNewTabPageBase mNewTabPage = (ChromeHomeNewTabPageBase) tab.getNativePage();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mNewTabPage.getCloseButtonForTests().callOnClick();
                getActivity().getLayoutManager().getActiveLayout().finishAnimationsForTests();
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
            assertEquals(0f, mFadingBackgroundView.getAlpha());
        } else {
            assertTrue(mFadingBackgroundView.isEnabled());
        }
    }
}
