// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.os.Handler;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.Tab;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.ui.touch_selection.SelectionEventType;

/**
 * Controls selection gesture interaction for Contextual Search.
 */
public class ContextualSearchSelectionController {

    /**
     * The type of selection made by the user.
     */
    public enum SelectionType {
        UNDETERMINED,
        TAP,
        LONG_PRESS
    }

    // The number of milliseconds to wait for a selection change after a tap before considering
    // the tap invalid.  This can't be too small or the subsequent taps may not have established
    // a new selection in time.  This is because selectWordAroundCaret doesn't always select.
    // TODO(donnd): Fix in Blink, crbug.com/435778.
    private static final int INVALID_IF_NO_SELECTION_CHANGE_AFTER_TAP_MS = 50;
    private static final double RETAP_DISTANCE_SQUARED_DP = Math.pow(75, 2);

    private final ChromeActivity mActivity;
    private final ContextualSearchSelectionHandler mHandler;
    private final Runnable mHandleInvalidTapRunnable;
    private final Handler mRunnableHandler;
    private final float mPxToDp;

    private String mSelectedText;
    private SelectionType mSelectionType;
    private boolean mWasTapGestureDetected;
    private boolean mIsSelectionBeingModified;
    private boolean mWasLastTapValid;
    private boolean mIsWaitingForInvalidTapDetection;

    private float mX;
    private float mY;

    private class ContextualSearchGestureStateListener extends GestureStateListener {
        @Override
        public void onScrollStarted(int scrollOffsetY, int scrollExtentY) {
            mHandler.handleScroll();
        }

        // TODO(donnd): Remove this once we get notification of the selection changing
        // after a tap-select gets a subsequent tap nearby.  Currently there's no
        // notification in this case.
        // See crbug.com/444114.
        @Override
        public void onSingleTap(boolean consumed, int x, int y) {
            // We may be notified that a tap has happened even when the system consumed the event.
            // This is being considered for support for tapping an existing selection to show the
            // pins.  We should only process this tap if it has not been consumed by the system.
            if (!consumed) scheduleInvalidTapNotification();
        }
    }

    /**
     * Constructs a new Selection controller for the given activity.  Callbacks will be issued
     * through the given selection handler.
     * @param activity The {@link ChromeActivity} to control.
     * @param handler The handler for callbacks.
     */
    public ContextualSearchSelectionController(ChromeActivity activity,
            ContextualSearchSelectionHandler handler) {
        mActivity = activity;
        mHandler = handler;
        mPxToDp = 1.f / mActivity.getResources().getDisplayMetrics().density;

        mRunnableHandler = new Handler();
        mHandleInvalidTapRunnable = new Runnable() {
            @Override
            public void run() {
                onInvalidTapDetectionTimeout();
            }
        };
    }

    /**
     * Returns a new {@code GestureStateListener} that will listen for events in the Base Page.
     * This listener will handle all Contextual Search-related interactions that go through the
     * listener.
     */
    public ContextualSearchGestureStateListener getGestureStateListener() {
        return new ContextualSearchGestureStateListener();
    }

    /**
     * @return the type of the selection.
     */
    SelectionType getSelectionType() {
        return mSelectionType;
    }

    /**
     * @return the selected text.
     */
    String getSelectedText() {
        return mSelectedText;
    }

    /**
     * Clears the selection.
     */
    void clearSelection() {
        ContentViewCore baseContentView = getBaseContentView();
        if (baseContentView != null) {
            baseContentView.clearSelection();
        }
        mHandler.onClearSelection();

        resetAllStates();
    }

    /**
     * Handles a change in the current Selection.
     * @param selection The selection portion of the context.
     */
    void handleSelectionChanged(String selection) {
        if (selection == null || selection.isEmpty()) {
            scheduleInvalidTapNotification();
            // When the user taps on the page it will place the caret in that position, which
            // will trigger a onSelectionChanged event with an empty string.
            if (mSelectionType == SelectionType.TAP) {
                // Since we mostly ignore a selection that's empty, we only need to partially reset.
                resetSelectionStates();
                return;
            }
        }
        if (selection != null && !selection.isEmpty()) {
            unscheduleInvalidTapNotification();
        }
        if (mIsSelectionBeingModified) {
            mSelectedText = selection;
            mHandler.handleSelectionModification(selection, mX, mY);
        } else if (mWasTapGestureDetected) {
            mSelectedText = selection;
            mSelectionType = SelectionType.TAP;
            mHandler.handleSelection(selection, mSelectionType, mX, mY);
            mWasTapGestureDetected = false;
        }
    }

    /**
     * Handles a notification that a selection event took place.
     * @param eventType The type of event that took place.
     * @param posXPix The x coordinate of the selection start handle.
     * @param posYPix The y coordinate of the selection start handle.
     */
    void handleSelectionEvent(int eventType, float posXPix, float posYPix) {
        boolean shouldHandleSelection = false;
        switch (eventType) {
            case SelectionEventType.SELECTION_SHOWN:
                mWasTapGestureDetected = false;
                mSelectionType = SelectionType.LONG_PRESS;
                shouldHandleSelection = true;
                break;
            case SelectionEventType.SELECTION_CLEARED:
                mHandler.onClearSelection();
                resetAllStates();
                break;
            case SelectionEventType.SELECTION_DRAG_STARTED:
                mIsSelectionBeingModified = true;
                break;
            case SelectionEventType.SELECTION_DRAG_STOPPED:
                mIsSelectionBeingModified = false;
                shouldHandleSelection = true;
                break;
            default:
        }

        if (shouldHandleSelection) {
            ContentViewCore baseContentView = getBaseContentView();
            if (baseContentView != null) {
                String selection = baseContentView.getSelectedText();
                if (selection != null) {
                    mX = posXPix;
                    mY = posYPix;
                    mSelectedText = selection;
                    mHandler.handleSelection(selection, SelectionType.LONG_PRESS, mX, mY);
                }
            }
        }
    }

    /**
     * Resets all internal state of this class, including the tap state.
     */
    private void resetAllStates() {
        resetSelectionStates();
        mWasLastTapValid = false;
    }

    /**
     * Resets all of the internal state of this class that handles the selection.
     */
    private void resetSelectionStates() {
        mSelectionType = SelectionType.UNDETERMINED;
        mSelectedText = null;

        mWasTapGestureDetected = false;
        mIsSelectionBeingModified = false;
    }

    /**
     * Handles an unhandled tap gesture.
     */
    void handleShowUnhandledTapUIIfNeeded(int x, int y) {
        mWasTapGestureDetected = false;
        if (mSelectionType != SelectionType.LONG_PRESS && shouldHandleTap(x, y)) {
            mX = x;
            mY = y;
            mWasLastTapValid = true;
            mWasTapGestureDetected = true;
            // TODO(donnd): Find a better way to determine that a navigation will be triggered
            // by the tap, or merge with other time-consuming actions like gathering surrounding
            // text or detecting page mutations.
            new Handler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    mHandler.handleValidTap();
                }
            }, ContextualSearchFieldTrial.getNavigationDetectionDelay());
        }
        if (!mWasTapGestureDetected) {
            mWasLastTapValid = false;
            mHandler.handleInvalidTap();
        }
    }

    /**
     * @return The Base Page's {@link ContentViewCore}, or {@code null} if there is no current tab.
     */
    ContentViewCore getBaseContentView() {
        Tab currentTab = mActivity.getActivityTab();
        return currentTab != null ? currentTab.getContentViewCore() : null;
    }

    /**
     * @return whether a tap at the given coordinates should be handled or not.
     */
    private boolean shouldHandleTap(int x, int y) {
        return !mWasLastTapValid || wasTapCloseToPreviousTap(x, y);
    }

    /**
     * Determines whether a tap at the given coordinates is considered "close" to the previous
     * tap.
     */
    private boolean wasTapCloseToPreviousTap(int x, int y) {
        float deltaXDp = (mX - x) * mPxToDp;
        float deltaYDp = (mY - y) * mPxToDp;
        float distanceSquaredDp =  deltaXDp * deltaXDp + deltaYDp * deltaYDp;
        return distanceSquaredDp <= RETAP_DISTANCE_SQUARED_DP;
    }

    /**
     * Schedules a notification to check if the tap was invalid.
     * When we call selectWordAroundCaret it selects nothing in cases where the tap was invalid.
     * We have no way to know other than scheduling a notification to check later.
     * This allows us to hide the bar when there's no selection.
     */
    private void scheduleInvalidTapNotification() {
        // TODO(donnd): Fix selectWordAroundCaret to we can tell if it selects, instead
        // of using a timer here!  See crbug.com/435778.
        mRunnableHandler.postDelayed(mHandleInvalidTapRunnable,
                INVALID_IF_NO_SELECTION_CHANGE_AFTER_TAP_MS);
    }

    /**
     * Un-schedules all pending notifications to check if a tap was invalid.
     */
    private void unscheduleInvalidTapNotification() {
        mRunnableHandler.removeCallbacks(mHandleInvalidTapRunnable);
        mIsWaitingForInvalidTapDetection = true;
    }

    /**
     * Notify's the system that tap gesture has been completed.
     */
    private void onInvalidTapDetectionTimeout() {
        mHandler.handleInvalidTap();
        mIsWaitingForInvalidTapDetection = false;
    }

    /**
     * @return whether a tap gesture has been detected, for testing.
     */
    @VisibleForTesting
    boolean wasAnyTapGestureDetected() {
        return mIsWaitingForInvalidTapDetection;
    }
}
