// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.support.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * Records user actions and histograms related to the {@link BottomSheet}.
 */
public class BottomSheetMetrics extends EmptyBottomSheetObserver {
    /**
     * The different ways that the bottom sheet can be opened. This is used to back a UMA
     * histogram and should therefore be treated as append-only.
     */
    @IntDef({OPENED_BY_SWIPE, OPENED_BY_OMNIBOX_FOCUS, OPENED_BY_NEW_TAB_CREATION,
            OPENED_BY_EXPAND_BUTTON, OPENED_BY_STARTUP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SheetOpenReason {}
    public static final int OPENED_BY_SWIPE = 0;
    public static final int OPENED_BY_OMNIBOX_FOCUS = 1;
    public static final int OPENED_BY_NEW_TAB_CREATION = 2;
    public static final int OPENED_BY_EXPAND_BUTTON = 3;
    public static final int OPENED_BY_STARTUP = 4;
    private static final int OPENED_BY_BOUNDARY = 5;

    /** The different ways that the bottom sheet can be closed. */
    @IntDef({CLOSED_BY_NONE, CLOSED_BY_SWIPE, CLOSED_BY_BACK_PRESS, CLOSED_BY_TAP_SCRIM,
            CLOSED_BY_NAVIGATION})
    public @interface SheetCloseReason {}
    private static final int CLOSED_BY_NONE = -1;
    public static final int CLOSED_BY_SWIPE = 0;
    public static final int CLOSED_BY_BACK_PRESS = 1;
    public static final int CLOSED_BY_TAP_SCRIM = 2;
    public static final int CLOSED_BY_NAVIGATION = 3;

    /** Whether the sheet is currently open. */
    private boolean mIsSheetOpen;

    /** The last {@link BottomSheetContent} that was displayed. */
    private BottomSheetContent mLastContent;

    /**
     * The current reason the sheet might become closed. This may change before the sheet actually
     * reaches the closed state.
     */
    @SheetCloseReason
    private int mSheetCloseReason;

    /** When this class was created. Used as a proxy for when the app was started. */
    private long mCreationTime;

    /** The last time the sheet was opened. */
    private long mLastOpenTime;

    /** The last time the sheet was closed. */
    private long mLastCloseTime;

    public BottomSheetMetrics() {
        mCreationTime = System.currentTimeMillis();
    }

    @Override
    public void onSheetOpened() {
        mIsSheetOpen = true;

        boolean isFirstOpen = mLastOpenTime == 0;
        mLastOpenTime = System.currentTimeMillis();

        if (isFirstOpen) {
            RecordHistogram.recordMediumTimesHistogram("Android.ChromeHome.TimeToFirstOpen",
                    mLastOpenTime - mCreationTime, TimeUnit.MILLISECONDS);
        } else {
            RecordHistogram.recordMediumTimesHistogram(
                    "Android.ChromeHome.TimeBetweenCloseAndNextOpen",
                    mLastOpenTime - mLastCloseTime, TimeUnit.MILLISECONDS);
        }
    }

    @Override
    public void onSheetClosed() {
        mIsSheetOpen = false;
        recordSheetCloseReason(mSheetCloseReason);
        mSheetCloseReason = CLOSED_BY_NONE;

        mLastCloseTime = System.currentTimeMillis();
        RecordHistogram.recordMediumTimesHistogram("Android.ChromeHome.DurationOpen",
                mLastCloseTime - mLastOpenTime, TimeUnit.MILLISECONDS);
    }

    @Override
    public void onSheetStateChanged(int newState) {
        if (newState == BottomSheet.SHEET_STATE_HALF) {
            RecordUserAction.record("Android.ChromeHome.HalfState");
        } else if (newState == BottomSheet.SHEET_STATE_FULL) {
            RecordUserAction.record("Android.ChromeHome.FullState");
        }
    }

    @Override
    public void onSheetContentChanged(BottomSheetContent newContent) {
        // Return early if the sheet content is being set during initialization (previous content
        // is null) or while the sheet is closed (sheet content being reset), so that we only
        // record actions when the user explicitly takes an action.
        if (mLastContent == null || !mIsSheetOpen) {
            mLastContent = newContent;
            return;
        }

        if (newContent.getType() == BottomSheetContentController.TYPE_SUGGESTIONS) {
            RecordUserAction.record("Android.ChromeHome.ShowSuggestions");
        } else if (newContent.getType() == BottomSheetContentController.TYPE_DOWNLOADS) {
            RecordUserAction.record("Android.ChromeHome.ShowDownloads");
        } else if (newContent.getType() == BottomSheetContentController.TYPE_BOOKMARKS) {
            RecordUserAction.record("Android.ChromeHome.ShowBookmarks");
        } else if (newContent.getType() == BottomSheetContentController.TYPE_HISTORY) {
            RecordUserAction.record("Android.ChromeHome.ShowHistory");
        } else if (newContent.getType() == BottomSheetContentController.TYPE_INCOGNITO_HOME) {
            RecordUserAction.record("Android.ChromeHome.ShowIncognitoHome");
        } else if (newContent.getType() == BottomSheetContentController.TYPE_PLACEHOLDER) {
            // Intentionally do nothing; the placeholder is not user triggered.
        } else {
            assert false;
        }
        mLastContent = newContent;
    }

    /**
     * Set the reason the bottom sheet is currently closing. This value is not recorded until after
     * the sheet is actually closed.
     * @param reason The {@link SheetCloseReason} that the sheet is closing.
     */
    public void setSheetCloseReason(@SheetCloseReason int reason) {
        mSheetCloseReason = reason;
    }

    /**
     * Records the reason the sheet was opened.
     * @param reason The {@link SheetOpenReason} that caused the bottom sheet to open.
     */
    public void recordSheetOpenReason(@SheetOpenReason int reason) {
        switch (reason) {
            case OPENED_BY_SWIPE:
                RecordUserAction.record("Android.ChromeHome.OpenedBySwipe");
                break;
            case OPENED_BY_OMNIBOX_FOCUS:
                RecordUserAction.record("Android.ChromeHome.OpenedByOmnibox");
                break;
            case OPENED_BY_NEW_TAB_CREATION:
                RecordUserAction.record("Android.ChromeHome.OpenedByNTP");
                break;
            case OPENED_BY_EXPAND_BUTTON:
                RecordUserAction.record("Android.ChromeHome.OpenedByExpandButton");
                break;
            case OPENED_BY_STARTUP:
                RecordUserAction.record("Android.ChromeHome.OpenedByStartup");
                break;
            default:
                assert false;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Android.ChromeHome.OpenReason", reason, OPENED_BY_BOUNDARY);
    }

    /**
     * Records the reason the sheet was closed.
     * @param reason The {@link SheetCloseReason} that cause the bottom sheet to close.
     */
    public void recordSheetCloseReason(@SheetCloseReason int reason) {
        switch (reason) {
            case CLOSED_BY_SWIPE:
                RecordUserAction.record("Android.ChromeHome.ClosedBySwipe");
                break;
            case CLOSED_BY_BACK_PRESS:
                RecordUserAction.record("Android.ChromeHome.ClosedByBackPress");
                break;
            case CLOSED_BY_TAP_SCRIM:
                RecordUserAction.record("Android.ChromeHome.ClosedByTapScrim");
                break;
            case CLOSED_BY_NAVIGATION:
                RecordUserAction.record("Android.ChromeHome.ClosedByNavigation");
                break;
            case CLOSED_BY_NONE:
                RecordUserAction.record("Android.ChromeHome.Closed");
                break;
            default:
                assert false;
        }
    }

    /**
     * Records that a user navigation instructed the NativePageFactory to create a native page for
     * the NTP. This may occur if the user has NTP URLs in a tab's navigation history.
     */
    public void recordNativeNewTabPageShown() {
        RecordUserAction.record("Android.ChromeHome.NativeNTPShown");
    }

    /**
     * Records that the user tapped the app menu item that triggers the in-product help bubble.
     */
    public void recordInProductHelpMenuItemClicked() {
        RecordUserAction.record("Android.ChromeHome.IPHMenuItemClicked");
    }
}
