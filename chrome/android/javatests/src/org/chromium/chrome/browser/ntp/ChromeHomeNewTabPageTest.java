// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.tab.Tab;
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
    private StateChangeBottomSheetObserver mObserver;
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

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mObserver = new StateChangeBottomSheetObserver();
        mBottomSheet.addObserver(mObserver);

        mFadingBackgroundView = getActivity().getFadingBackgroundView();

        // Once setup is done, get the initial call count for onStateChanged().
        mStateChangeCurrentCalls = mObserver.mStateChangedCallbackHelper.getCallCount();
    }

    @SmallTest
    public void testCloseNTP_OneTab()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        // Load the NTP.
        Tab tab = getActivity().getActivityTab();
        loadUrl(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(tab);

        validateState(true, true);

        // Close the new tab.
        closeNewTab();
        assertEquals(0, getActivity().getTabModelSelector().getTotalTabCount());
        assertFalse("Overview mode should not be showing",
                getActivity().getLayoutManager().overviewVisible());
    }

    @SmallTest
    public void testCloseNTP_TwoTabs()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        // Create a new tab.
        createNewTab();

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
        createNewTab();

        // Close the new tab.
        closeNewTab();
        assertEquals(1, getActivity().getTabModelSelector().getTotalTabCount());
        assertTrue("Overview mode should be showing",
                getActivity().getLayoutManager().overviewVisible());
    }

    @SmallTest
    public void testToggleSelectedTab()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        // Create a new tab.
        createNewTab();

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

        validateState(false, false);

        // Select the NTP.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().getCurrentTabModel().setIndex(1, TabSelectionType.FROM_USER);
            }
        });

        validateState(true, true);
    }

    private void createNewTab() throws InterruptedException, TimeoutException {
        ChromeTabUtils.fullyLoadUrlInNewTab(
                getInstrumentation(), getActivity(), UrlConstants.NTP_URL, false);
        validateState(true, true);
    }

    private void closeNewTab() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();
        final ChromeHomeNewTabPage mNewTabPage = (ChromeHomeNewTabPage) tab.getNativePage();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mNewTabPage.getCloseButtonForTests().callOnClick();
            }
        });

        validateState(false, true);
    }

    private void validateState(boolean newTabPageSelected, boolean animatesToState)
            throws InterruptedException, TimeoutException {
        // Wait for two calls if animating; one is to SHEET_STATE_SCROLLING and the other is to the
        // final state.
        mObserver.mStateChangedCallbackHelper.waitForCallback(
                mStateChangeCurrentCalls, animatesToState ? 2 : 1);

        if (newTabPageSelected) {
            assertEquals("Sheet should be at half height", BottomSheet.SHEET_STATE_HALF,
                    mBottomSheet.getSheetState());
            assertFalse(mFadingBackgroundView.isEnabled());
            assertEquals(0f, mFadingBackgroundView.getAlpha());
        } else {
            assertEquals("Sheet should be peeking", BottomSheet.SHEET_STATE_PEEK,
                    mBottomSheet.getSheetState());
            assertTrue(mFadingBackgroundView.isEnabled());
        }

        // Once the state is validated, update the call count.
        mStateChangeCurrentCalls = mObserver.mStateChangedCallbackHelper.getCallCount();
    }
}
