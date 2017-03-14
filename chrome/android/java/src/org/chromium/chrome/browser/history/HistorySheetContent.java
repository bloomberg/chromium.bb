// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.view.View;

import org.chromium.chrome.browser.widget.BottomSheet.BottomSheetContent;

/**
 * A {@link BottomSheetContent} holding a {@link HistoryManager} for display in the BottomSheet.
 */
public class HistorySheetContent implements BottomSheetContent {
    private final View mContentView;
    private HistoryManager mHistoryManager;

    /**
     * @param activity The activity displaying the history manager UI.
     */
    public HistorySheetContent(Activity activity) {
        mHistoryManager = new HistoryManager(activity, null);
        mContentView = mHistoryManager.detachContentView();
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mHistoryManager.getVerticalScrollOffset();
    }

    @Override
    public void destroy() {
        mHistoryManager.onDestroyed();
        mHistoryManager = null;
    }
}
