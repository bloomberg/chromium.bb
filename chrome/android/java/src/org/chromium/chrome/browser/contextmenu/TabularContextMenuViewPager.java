// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.support.v4.view.ViewPager;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.chrome.R;

/**
 * When there is more than one view for the context menu to display, it wraps the display in a view
 * pager.
 */
public class TabularContextMenuViewPager extends ViewPager {
    private final int mContextMenuMinimumPaddingPx =
            getResources().getDimensionPixelSize(R.dimen.context_menu_min_padding);

    public TabularContextMenuViewPager(Context context) {
        super(context);
    }

    public TabularContextMenuViewPager(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Used to show the full ViewPager dialog. Without this the dialog would have no height or
     * width.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int menuHeight = 0;
        int tabHeight = 0;

        // getCurrentItem() does not take into account the tab layout unlike getChildCount().
        int currentItemsIndex = getCurrentItem() + 1;

        // The width of the context menu is defined so that it leaves space between itself and the
        // screen's edges. It is also bounded to a max size to prevent the menu from stretching
        // across a large display (e.g. a tablet screen).
        int deviceWidthPx = getResources().getDisplayMetrics().widthPixels;
        int contextMenuWidth = Math.min(deviceWidthPx - 2 * mContextMenuMinimumPaddingPx,
                getResources().getDimensionPixelSize(R.dimen.context_menu_max_width));

        widthMeasureSpec = MeasureSpec.makeMeasureSpec(contextMenuWidth, MeasureSpec.EXACTLY);

        // The height of the context menu is calculated as the sum of:
        // 1. The tab bar's height, which is only visible when the context menu requires it
        //    (i.e. an ImageLink is clicked)
        // 2. The height of the View being displayed for the current tab.
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);

            child.measure(
                    widthMeasureSpec, MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
            int measuredHeight = child.getMeasuredHeight();

            // The ViewPager also considers the tab layout one of its children, and needs to be
            // treated separately from getting the largest height.
            if (child.getId() == R.id.tab_layout && child.getVisibility() != GONE) {
                tabHeight = measuredHeight;
            } else if (i == currentItemsIndex) {
                menuHeight = child.getMeasuredHeight();
                break;
            }
        }

        // Cap the height of the context menu so that it fits on the screen without touching the
        // screen's edges.
        int fullHeight = menuHeight + tabHeight;
        int deviceHeightPx = getResources().getDisplayMetrics().heightPixels;
        fullHeight = Math.min(fullHeight, deviceHeightPx - 2 * mContextMenuMinimumPaddingPx);

        heightMeasureSpec = MeasureSpec.makeMeasureSpec(fullHeight, MeasureSpec.EXACTLY);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
