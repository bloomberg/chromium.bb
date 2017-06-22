// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import static org.chromium.chrome.browser.ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE;
import static org.chromium.chrome.test.BottomSheetTestRule.ENABLE_CHROME_HOME;
import static org.chromium.chrome.test.ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG;

import android.support.v7.widget.RecyclerView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContentController;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;

/**
 * Junit4 rule for tests testing the Chrome Home bottom sheet.
 */
@CommandLineFlags.Add({ENABLE_CHROME_HOME, DISABLE_FIRST_RUN_EXPERIENCE,
        DISABLE_NETWORK_PREDICTION_FLAG})
public class BottomSheetTestRule extends ChromeTabbedActivityTestRule {
    /** An observer used to record events that occur with respect to the bottom sheet. */
    public static class Observer extends EmptyBottomSheetObserver {
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

    public static final String ENABLE_CHROME_HOME =
            "enable-features=" + ChromeFeatureList.CHROME_HOME;

    /** A handle to the sheet's observer. */
    private Observer mObserver;

    /** A handle to the bottom sheet. */
    private BottomSheet mBottomSheet;

    /** A handle to the {@link BottomSheetContentController}. */
    private BottomSheetContentController mBottomSheetContentController;

    private boolean mOldChromeHomeFlagValue;

    protected void beforeStartingActivity() {
        // TODO(dgn,mdjones): Chrome restarts when the ChromeHome feature flag value changes. That
        // crashes the test so we need to manually set the preference to match the flag before
        // staring Chrome.
        ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();
        mOldChromeHomeFlagValue = prefManager.isChromeHomeEnabled();
        prefManager.setChromeHomeEnabled(true);
    }

    protected void afterStartingActivity() {
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

        mObserver = new Observer();
        mBottomSheet.addObserver(mObserver);
    }

    @Override
    protected void afterActivityFinished() {
        super.afterActivityFinished();
        ChromePreferenceManager.getInstance().setChromeHomeEnabled(mOldChromeHomeFlagValue);
    }

    // TODO (aberent): The Chrome test rules currently bypass ActivityTestRule.launchActivity, hence
    // don't call beforeActivityLaunched and afterActivityLaunched as defined in the
    // ActivityTestRule interface. To work round this override the methods that start activities.
    // See https://crbug.com/726444.
    @Override
    public void startMainActivityOnBlankPage() throws InterruptedException {
        beforeStartingActivity();
        super.startMainActivityOnBlankPage();
        afterStartingActivity();
    }

    public Observer getObserver() {
        return mObserver;
    }

    public BottomSheet getBottomSheet() {
        return mBottomSheet;
    }

    public BottomSheetContentController getBottomSheetContentController() {
        return mBottomSheetContentController;
    }

    /**
     * Set the bottom sheet's state on the UI thread.
     *
     * @param state   The state to set the sheet to.
     * @param animate If the sheet should animate to the provided state.
     */
    public void setSheetState(final int state, final boolean animate) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.setSheetState(state, animate);
            }
        });
    }

    /**
     * Set the bottom sheet's offset from the bottom of the screen on the UI thread.
     *
     * @param offset The offset from the bottom that the sheet should be.
     */
    public void setSheetOffsetFromBottom(final float offset) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.setSheetOffsetFromBottomForTesting(offset);
            }
        });
    }

    public BottomSheetContent getBottomSheetContent() {
        return getActivity().getBottomSheet().getCurrentSheetContent();
    }

    /**
     * @param itemId The id of the MenuItem corresponding to the {@link BottomSheetContent} to
     *               select.
     */
    public void selectBottomSheetContent(final int itemId) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheetContentController.selectItem(itemId);
            }
        });
    }
}
