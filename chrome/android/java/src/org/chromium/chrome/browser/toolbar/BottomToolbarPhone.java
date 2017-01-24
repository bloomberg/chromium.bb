// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.BottomSheet;

/**
 * Phone specific toolbar that exists at the bottom of the screen.
 */
public class BottomToolbarPhone extends ToolbarPhone {

    /** A handle to the bottom sheet. */
    private BottomSheet mBottomSheet;

    /** The state of the bottom sheet before the URL bar was focused. */
    private int mStateBeforeUrlFocus;

    /**
     * Constructs a BottomToolbarPhone object.
     * @param context The Context in which this View object is created.
     * @param attrs The AttributeSet that was specified with this View.
     */
    public BottomToolbarPhone(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected int getProgressBarTopMargin() {
        // In the case where the toolbar is at the bottom of the screen, the progress bar should
        // be at the top of the toolbar.
        return getContext().getResources().getDimensionPixelSize(R.dimen.toolbar_shadow_height);
    }

    @Override
    protected void triggerUrlFocusAnimation(final boolean hasFocus) {
        super.triggerUrlFocusAnimation(hasFocus);

        if (mBottomSheet == null) return;

        if (hasFocus) mStateBeforeUrlFocus = mBottomSheet.getSheetState();
        mBottomSheet.setSheetState(
                hasFocus ? BottomSheet.SHEET_STATE_FULL : mStateBeforeUrlFocus, true);
    }

    @Override
    public void setBottomSheet(BottomSheet sheet) {
        mBottomSheet = sheet;
    }
}
