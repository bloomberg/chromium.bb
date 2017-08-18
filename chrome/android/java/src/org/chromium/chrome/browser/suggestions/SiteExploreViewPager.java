// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.content.Context;
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
        View tabLayout = getChildAt(0);
        tabLayout.measure(
                widthMeasureSpec, MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        int tabLayoutHeight = tabLayout.getMeasuredHeight();

        int maxChildHeight = 0;
        for (int i = 1; i < getChildCount(); i++) {
            View child = getChildAt(i);
            child.measure(
                    widthMeasureSpec, MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
            int childHeight = child.getMeasuredHeight();
            if (childHeight > maxChildHeight) maxChildHeight = childHeight;
        }

        heightMeasureSpec =
                MeasureSpec.makeMeasureSpec(maxChildHeight + tabLayoutHeight, MeasureSpec.EXACTLY);

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
