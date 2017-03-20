// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;

/**
 * Records user action and histograms related to the {@link BottomSheet}.
 */
public class BottomSheetMetrics extends EmptyBottomSheetObserver {
    private boolean mIsSheetOpen;
    private BottomSheetContent mLastContent;

    @Override
    public void onSheetOpened() {
        mIsSheetOpen = true;
        RecordUserAction.record("Android.ChromeHome.Opened");
    }

    @Override
    public void onSheetClosed() {
        mIsSheetOpen = false;
        RecordUserAction.record("Android.ChromeHome.Closed");
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
        } else {
            assert false;
        }
        mLastContent = newContent;
    }

}
