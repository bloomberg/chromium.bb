// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.widget.FadingBackgroundView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.chrome.test.BottomSheetTestCaseBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the {@link ChromeHomeNewTabPage}.
 */
public class ChromeHomeNewTabPageTest extends BottomSheetTestCaseBase {
    private FadingBackgroundView mFadingBackgroundView;
    private StateChangeBottomSheetObserver mBottomSheetObserver;
    private TestTabModelObserver mTabModelObserver;
    private int mStateChangeCurrentCalls;

    /** On observer used to record state change events on the bottom sheet. */
    private static class StateChangeBottomSheetObserver extends EmptyBottomSheetObserver {
        /** A {@link CallbackHelper} that waits for the bottom sheet state to change. */
        private final CallbackHelper mStateChangedCallbackHelper = new CallbackHelper();

        @Override
        public void onSheetStateChanged(int state) {
            mStateChangedCallbackHelper.notifyCalled();
        }
    }

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

        mBottomSheetObserver = new StateChangeBottomSheetObserver();
        mBottomSheet.addObserver(mBottomSheetObserver);

        mTabModelObserver = new TestTabModelObserver();
        getActivity().getTabModelSelector().getModel(false).addObserver(mTabModelObserver);
        getActivity().getTabModelSelector().getModel(true).addObserver(mTabModelObserver);

        mFadingBackgroundView = getActivity().getFadingBackgroundView();

        // Once setup is done, get the initial call count for onStateChanged().
        mStateChangeCurrentCalls = mBottomSheetObserver.mStateChangedCallbackHelper.getCallCount();
    }

    @SmallTest
    @DisabledTest(message = "crbug.com/720637")
    public void testCloseNTP_OneTab()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        // Load the NTP.
        Tab tab = getActivity().getActivityTab();
        loadUrl(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(tab);

        validateState(true, true, true);

        // Close the new tab.
        closeNewTab();
        assertEquals(0, getActivity().getTabModelSelector().getTotalTabCount());
        assertTrue("Overview mode should be showing",
                getActivity().getLayoutManager().overviewVisible());
    }

    @SmallTest
    public void testCloseNTP_TwoTabs()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        // Create a new tab.
        createNewTab(false);

        // Close the new tab.
        closeNewTab();
        assertEquals(1, getActivity().getTabModelSelector().getTotalTabCount());
        assertFalse("Overview mode should not be showing",
                getActivity().getLayoutManager().overviewVisible());
    }

    @SmallTest
    public void testCloseNTP_TwoTabs_OverviewMode()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        // Switch to overview mode.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().getLayoutManager().showOverview(false);
            }
        });

        // Create a new tab.
        createNewTab(false);

        // Close the new tab.
        closeNewTab();
        assertEquals(1, getActivity().getTabModelSelector().getTotalTabCount());
        assertTrue("Overview mode should be showing",
                getActivity().getLayoutManager().overviewVisible());
    }

    @SmallTest
    public void testCloseNTP_Incognito()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        // Create new incognito NTP.
        createNewTab(true);

        // Close the new tab.
        closeNewTab();
        assertEquals(1, getActivity().getTabModelSelector().getTotalTabCount());
        assertFalse("Overview mode should not be showing",
                getActivity().getLayoutManager().overviewVisible());
    }

    @SmallTest
    public void testToggleSelectedTab()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        // Create a new tab.
        createNewTab(false);

        // Select the original tab.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // It's not possible for the user to select a new tab while the bottom sheet is
                // open.
                getActivity().getBottomSheet().setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
                getActivity().getCurrentTabModel().setIndex(0, TabSelectionType.FROM_USER);
            }
        });

        validateState(false, false, true);

        // Select the NTP.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().getCurrentTabModel().setIndex(1, TabSelectionType.FROM_USER);
            }
        });

        validateState(true, false, false);
    }

    private void createNewTab(boolean incognito) throws InterruptedException, TimeoutException {
        ChromeTabUtils.fullyLoadUrlInNewTab(
                getInstrumentation(), getActivity(), UrlConstants.NTP_URL, incognito);
        validateState(true, true, true);
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

        validateState(false, true, true);
    }

    private void validateState(boolean newTabPageSelected, boolean animatesToState,
            boolean bottomSheetStateChanging) throws InterruptedException, TimeoutException {
        // Wait for two calls if animating; one is to SHEET_STATE_SCROLLING and the other is to the
        // final state.
        if (bottomSheetStateChanging) {
            mBottomSheetObserver.mStateChangedCallbackHelper.waitForCallback(
                    mStateChangeCurrentCalls, animatesToState ? 2 : 1);
        }

        int expectedEndState = !bottomSheetStateChanging
                ? mBottomSheet.getSheetState()
                : newTabPageSelected ? BottomSheet.SHEET_STATE_HALF : BottomSheet.SHEET_STATE_PEEK;

        assertEquals("Sheet state incorrect", expectedEndState, mBottomSheet.getSheetState());

        if (newTabPageSelected) {
            assertFalse(mFadingBackgroundView.isEnabled());
            assertEquals(0f, mFadingBackgroundView.getAlpha());
        } else {
            assertTrue(mFadingBackgroundView.isEnabled());
        }

        // Once the state is validated, update the call count.
        mStateChangeCurrentCalls = mBottomSheetObserver.mStateChangedCallbackHelper.getCallCount();
    }
}
