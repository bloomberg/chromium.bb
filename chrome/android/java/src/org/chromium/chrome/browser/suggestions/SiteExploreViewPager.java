// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.content.Context;
import android.support.design.widget.TabLayout;
import android.support.v4.view.ViewPager;
import android.util.AttributeSet;
import android.view.View;

/**
 * View Pager for the Site Explore UI. This is needed in order to correctly calculate the height
 * of this section on {@link #onMeasure(int, int)}.
 */
public class SiteExploreViewPager extends ViewPager {
    public SiteExploreViewPager(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int tabLayoutHeight = 0;
        int tabIndicatorHeight = 0;
        int maxChildHeight = 0;
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);

            child.measure(
                    widthMeasureSpec, MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
            if (child instanceof TabLayout) {
                tabLayoutHeight = child.getMeasuredHeight();
            } else if (child instanceof SiteExplorePageIndicatorView) {
                tabIndicatorHeight = child.getMeasuredHeight();
            } else {
                int tileGridHeight = child.getMeasuredHeight();
                if (tileGridHeight > maxChildHeight) maxChildHeight = tileGridHeight;
            }
        }

        heightMeasureSpec = MeasureSpec.makeMeasureSpec(
                maxChildHeight + tabLayoutHeight + tabIndicatorHeight, MeasureSpec.EXACTLY);

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
