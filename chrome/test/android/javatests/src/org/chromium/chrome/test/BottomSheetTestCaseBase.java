// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_PHONE;

import android.support.v7.widget.RecyclerView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContentController;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;

/**
 * Base class for instrumentation tests using the bottom sheet.
 */
@CommandLineFlags.Add({"enable-features=ChromeHome"})
@Restriction(RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public abstract class BottomSheetTestCaseBase extends ChromeTabbedActivityTestBase {
    /** A handle to the sheet's observer. */
    protected TestBottomSheetObserver mObserver;

    /** An observer used to record events that occur with respect to the bottom sheet. */
    protected static class TestBottomSheetObserver extends EmptyBottomSheetObserver {
        /** A {@link CallbackHelper} that can wait for the bottom sheet to be closed. */
        public final CallbackHelper mClosedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the bottom sheet to be opened. */
        public final CallbackHelper mOpenedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the onTransitionPeekToHalf event. */
        public final CallbackHelper mPeekToHalfCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the onOffsetChanged event. */
        public final CallbackHelper mOffsetChangedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the onSheetContentChanged event. */
        public final CallbackHelper mContentChangedCallbackHelper = new CallbackHelper();

        /** The last value that the onTransitionPeekToHalf event sent. */
        private float mLastPeekToHalfValue;

        /** The last value that the onOffsetChanged event sent. */
        private float mLastOffsetChangedValue;

        @Override
        public void onTransitionPeekToHalf(float fraction) {
            mLastPeekToHalfValue = fraction;
            mPeekToHalfCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetOffsetChanged(float heightFraction) {
            mLastOffsetChangedValue = heightFraction;
            mOffsetChangedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetOpened() {
            mOpenedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetClosed() {
            mClosedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetContentChanged(BottomSheetContent newContent) {
            mContentChangedCallbackHelper.notifyCalled();
        }

        /** @return The last value passed in to {@link #onTransitionPeekToHalf(float)}. */
        public float getLastPeekToHalfValue() {
            return mLastPeekToHalfValue;
        }

        /** @return The last value passed in to {@link #onSheetOffsetChanged(float)}. */
        public float getLastOffsetChangedValue() {
            return mLastOffsetChangedValue;
        }
    }

    /** A handle to the bottom sheet. */
    protected BottomSheet mBottomSheet;

    /** A handle to the {@link BottomSheetContentController}. */
    protected BottomSheetContentController mBottomSheetContentController;

    private boolean mOldChromeHomeFlagValue;
    @Override
    protected void setUp() throws Exception {
        // TODO(dgn,mdjones): Chrome restarts when the ChromeHome feature flag value changes. That
        // crashes the test so we need to manually set the preference to match the flag before
        // Chrome initialises via super.setUp()
        ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();
        mOldChromeHomeFlagValue = prefManager.isChromeHomeEnabled();
        prefManager.setChromeHomeEnabled(true);

        super.setUp();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().getBottomSheet().setSheetState(
                        BottomSheet.SHEET_STATE_FULL, /* animate = */ false);
            }
        });
        // The default BottomSheetContent is SuggestionsBottomSheetContent, whose content view is a
        // RecyclerView.
        RecyclerViewTestUtils.waitForStableRecyclerView(
                ((RecyclerView) getBottomSheetContent().getContentView().findViewById(
                        R.id.recycler_view)));

        mBottomSheet = getActivity().getBottomSheet();
        mBottomSheetContentController = getActivity().getBottomSheetContentController();

        mObserver = new TestBottomSheetObserver();
        mBottomSheet.addObserver(mObserver);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        ChromePreferenceManager.getInstance().setChromeHomeEnabled(mOldChromeHomeFlagValue);
    }

    /**
     * Set the bottom sheet's state on the UI thread.
     * @param state The state to set the sheet to.
     * @param animate If the sheet should animate to the provided state.
     */
    protected void setSheetState(final int state, final boolean animate) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.setSheetState(state, animate);
            }
        });
    }

    /**
     * Set the bottom sheet's offset from the bottom of the screen on the UI thread.
     * @param offset The offset from the bottom that the sheet should be.
     */
    protected void setSheetOffsetFromBottom(final float offset) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.setSheetOffsetFromBottomForTesting(offset);
            }
        });
    }

    protected BottomSheetContent getBottomSheetContent() {
        return getActivity().getBottomSheet().getCurrentSheetContent();
    }

    /**
     * @param itemId The id of the MenuItem corresponding to the {@link BottomSheetContent} to
     *               select.
     */
    protected void selectBottomSheetContent(final int itemId) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheetContentController.selectItem(itemId);
            }
        });
    }
}
