// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.util.Pair;
import android.view.View.MeasureSpec;
import android.view.View;

import org.chromium.content.browser.ContentViewCore;

/**
 * Helper methods used to manage the layout of the View that contains AwContents.
 */
public class AwLayoutSizer {
    // These are used to prevent a re-layout if the content size changes within a dimension that is
    // fixed by the view system.
    private boolean mWidthMeasurementIsFixed;
    private boolean mHeightMeasurementIsFixed;

    // Size of the rendered content, as reported by native, in physical pixels.
    private int mContentHeight;
    private int mContentWidth;

    // Callback object for interacting with the View.
    Delegate mDelegate;

    public interface Delegate {
        void requestLayout();
        void setMeasuredDimension(int measuredWidth, int measuredHeight);
    }

    public AwLayoutSizer(Delegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Update the contents size.
     * This should be called whenever the content size changes (due to DOM manipulation or page
     * load, for example).
     * The width and height should be in physical pixels.
     */
    public void onContentSizeChanged(int width, int height) {
        boolean layoutNeeded = (mContentWidth != width && !mWidthMeasurementIsFixed) ||
            (mContentHeight != height && !mHeightMeasurementIsFixed);

        mContentWidth = width;
        mContentHeight = height;

        if (layoutNeeded) {
            mDelegate.requestLayout();
        }
    }

    /**
     * Calculate the size of the view.
     * This is designed to be used to implement the android.view.View#onMeasure() method.
     */
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int heightMode = MeasureSpec.getMode(heightMeasureSpec);
        int heightSize = MeasureSpec.getSize(heightMeasureSpec);
        int widthMode = MeasureSpec.getMode(widthMeasureSpec);
        int widthSize = MeasureSpec.getSize(widthMeasureSpec);

        int measuredHeight = heightSize;
        int measuredWidth = widthSize;

        // Always use the given size unless unspecified. This matches WebViewClassic behavior.
        mWidthMeasurementIsFixed = (widthMode != MeasureSpec.UNSPECIFIED);
        // Freeze the height if an exact size is given by the parent or if the content size has
        // exceeded the maximum size specified by the parent.
        // TODO(mkosiba): Actually we'd like the reduction in content size to cause the WebView to
        // shrink back again but only as a result of a page load.
        mHeightMeasurementIsFixed = (heightMode == MeasureSpec.EXACTLY) ||
            (heightMode == MeasureSpec.AT_MOST && mContentHeight > heightSize);

        if (!mHeightMeasurementIsFixed) {
            measuredHeight = mContentHeight;
        }

        if (!mWidthMeasurementIsFixed) {
            measuredWidth = mContentWidth;
        }

        if (measuredHeight < mContentHeight) {
            measuredHeight |= View.MEASURED_STATE_TOO_SMALL;
        }

        if (measuredWidth < mContentWidth) {
            measuredWidth |= View.MEASURED_STATE_TOO_SMALL;
        }

        mDelegate.setMeasuredDimension(measuredWidth, measuredHeight);
    }
}
