// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;

/**
 * SimpleHorizontalLayoutView is a fast and specialized horizontal layout view.
 * It is based on a premise that no more than one child view can expand dynamically, while all other
 * views must have fixed, predefined size.
 *
 * Principles of operation:
 * - Each fixed-size view must come with an associated LayoutParams structure.
 * - The dynamically-sized view must have LayoutParams structure unset.
 * - The height of the view will be the result of measurement of the dynamically sized view.
 */
class SimpleHorizontalLayoutView extends ViewGroup {
    /**
     * SimpleHorizontalLayoutView's LayoutParams.
     *
     * These parameters introduce additional value to be used with |width| parameter
     * that identifies object that should occupy remaining space.
     */
    static class LayoutParams extends ViewGroup.LayoutParams {
        /** Indicates a resizable view. */
        public boolean dynamic;

        LayoutParams(int width, int height) {
            super(width, height);
        }

        /**
         * Create LayoutParams for a dynamic, resizeable view.
         */
        static LayoutParams forDynamicView() {
            LayoutParams res = new LayoutParams(WRAP_CONTENT, WRAP_CONTENT);
            res.dynamic = true;
            return res;
        }
    }

    SimpleHorizontalLayoutView(Context context) {
        super(context);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        // Note: We layout children in the following order:
        // - first-to-last in LTR orientation and
        // - last-to-first in RTL orientation.
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        int index = isRtl ? getChildCount() - 1 : 0;
        int increment = isRtl ? -1 : 1;

        left = 0;

        for (; index >= 0 && index < getChildCount(); index += increment) {
            View v = getChildAt(index);
            if (v.getVisibility() == GONE) continue;
            v.layout(left, 0, left + v.getMeasuredWidth(), bottom - top);
            left += v.getMeasuredWidth();
        }
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        int contentViewWidth = MeasureSpec.getSize(widthSpec);
        View dynamicView = null;

        // Compute and apply space we can offer to content view.
        for (int index = 0; index < getChildCount(); ++index) {
            View v = getChildAt(index);

            LayoutParams p = (LayoutParams) v.getLayoutParams();

            // Do not take dynamic view into consideration when computing space for it.
            // We identify the dynamic view by its custom width parameter.
            if (p.dynamic) {
                assert dynamicView == null : "Only one dynamically sized view is permitted.";
                dynamicView = v;
                continue;
            }

            if (v.getVisibility() == GONE) continue;
            if (p.width > 0) contentViewWidth -= p.width;
        }

        assert dynamicView != null : "No content view specified";

        // Measure height of the content view given the width constraint.
        dynamicView.measure(MeasureSpec.makeMeasureSpec(contentViewWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int heightPx = dynamicView.getMeasuredHeight();
        heightSpec = MeasureSpec.makeMeasureSpec(heightPx, MeasureSpec.EXACTLY);

        // Apply measured dimensions to all children.
        for (int index = 0; index < getChildCount(); ++index) {
            View v = getChildAt(index);

            // Avoid calling (expensive) measure on dynamic view twice.
            if (v == dynamicView) continue;

            v.measure(MeasureSpec.makeMeasureSpec(v.getLayoutParams().width, MeasureSpec.EXACTLY),
                    heightSpec);
        }

        setMeasuredDimension(MeasureSpec.getSize(widthSpec), MeasureSpec.getSize(heightSpec));
    }
}
