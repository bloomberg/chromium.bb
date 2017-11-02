// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.view.View;
import android.view.View.MeasureSpec;

import org.chromium.base.CommandLine;
import org.chromium.content.common.ContentSwitches;

/**
 * Helper methods used to manage the layout of the View that contains AwContents.
 */
public class AwLayoutSizer {
    // These are used to prevent a re-layout if the content size changes within a dimension that is
    // fixed by the view system.
    private boolean mWidthMeasurementIsFixed;
    private boolean mHeightMeasurementIsFixed;

    // Size of the rendered content, as reported by native.
    private int mContentHeightDip;
    private int mContentWidthDip;

    // Page scale factor.
    private float mPageScaleFactor = 1.0f;

    // Whether to postpone layout requests.
    private boolean mFreezeLayoutRequests;
    // Did we try to request a layout since the last time mPostponeLayoutRequests was set to true.
    private boolean mFrozenLayoutRequestPending;

    private double mDIPScale;

    // Was our height larger than the AT_MOST constraint the last time onMeasure was called?
    private boolean mHeightMeasurementLimited;
    // If mHeightMeasurementLimited is true then this contains the height limit.
    private int mHeightMeasurementLimit;

    // Callback object for interacting with the View.
    private Delegate mDelegate;

    /**
     * Delegate interface through which the AwLayoutSizer communicates with the view it's sizing.
     */
    public interface Delegate {
        void requestLayout();
        void setMeasuredDimension(int measuredWidth, int measuredHeight);
        boolean isLayoutParamsHeightWrapContent();
        void setForceZeroLayoutHeight(boolean forceZeroHeight);
    }

    /**
     * Default constructor. Note: both setDelegate and setDIPScale must be called before the class
     * is ready for use.
     */
    public AwLayoutSizer() {
    }

    public void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }

    public void setDIPScale(double dipScale) {
        mDIPScale = dipScale;
    }

    /**
     * Postpone requesting layouts till unfreezeLayoutRequests is called.
     */
    public void freezeLayoutRequests() {
        mFreezeLayoutRequests = true;
        mFrozenLayoutRequestPending = false;
    }

    /**
     * Stop postponing layout requests and request layout if such a request would have been made
     * had the freezeLayoutRequests method not been called before.
     */
    public void unfreezeLayoutRequests() {
        mFreezeLayoutRequests = false;
        if (mFrozenLayoutRequestPending) {
            mFrozenLayoutRequestPending = false;
            mDelegate.requestLayout();
        }
    }

    /**
     * Update the contents size.
     * This should be called whenever the content size changes (due to DOM manipulation or page
     * load, for example).
     * The width and height should be in DIP pixels. When --use-zoom-for-dsf is disabled, the
     * given width and height are in CSS pixels and we must scale them by DIP scale.
     */
    public void onContentSizeChanged(int width, int height) {
        CommandLine commandLine = CommandLine.getInstance();
        if (!commandLine.hasSwitch(ContentSwitches.ENABLE_USE_ZOOM_FOR_DSF)) {
            width *= mDIPScale;
            height *= mDIPScale;
        }
        doUpdate(width, height, mPageScaleFactor);
    }

    /**
     * Update the contents page scale.
     * This should be called whenever the content page scale factor changes (due to pinch zoom, for
     * example).
     */
    public void onPageScaleChanged(float pageScaleFactor) {
        doUpdate(mContentWidthDip, mContentHeightDip, pageScaleFactor);
    }

    private void doUpdate(int widthDip, int heightDip, float pageScaleFactor) {
        // We want to request layout only if the size or scale change, however if any of the
        // measurements are 'fixed', then changing the underlying size won't have any effect, so we
        // ignore changes to dimensions that are 'fixed'.
        final int heightPix = (int) (heightDip * mPageScaleFactor);
        boolean pageScaleChanged = mPageScaleFactor != pageScaleFactor;
        boolean contentHeightChangeMeaningful = !mHeightMeasurementIsFixed
                && (!mHeightMeasurementLimited || heightPix < mHeightMeasurementLimit);
        boolean pageScaleChangeMeaningful =
                !mWidthMeasurementIsFixed || contentHeightChangeMeaningful;
        boolean layoutNeeded = (mContentWidthDip != widthDip && !mWidthMeasurementIsFixed)
                || (mContentHeightDip != heightDip && contentHeightChangeMeaningful)
                || (pageScaleChanged && pageScaleChangeMeaningful);

        mContentWidthDip = widthDip;
        mContentHeightDip = heightDip;
        mPageScaleFactor = pageScaleFactor;

        if (layoutNeeded) {
            if (mFreezeLayoutRequests) {
                mFrozenLayoutRequestPending = true;
            } else {
                mDelegate.requestLayout();
            }
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

        int contentHeightPix = (int) (mContentHeightDip * mPageScaleFactor);
        int contentWidthPix = (int) (mContentWidthDip * mPageScaleFactor);

        int measuredHeight = contentHeightPix;
        int measuredWidth = contentWidthPix;

        // Always use the given size unless unspecified. This matches WebViewClassic behavior.
        mWidthMeasurementIsFixed = (widthMode != MeasureSpec.UNSPECIFIED);
        mHeightMeasurementIsFixed = (heightMode == MeasureSpec.EXACTLY);
        mHeightMeasurementLimited =
            (heightMode == MeasureSpec.AT_MOST) && (contentHeightPix > heightSize);
        mHeightMeasurementLimit = heightSize;

        if (mHeightMeasurementIsFixed || mHeightMeasurementLimited) {
            measuredHeight = heightSize;
        }

        if (mWidthMeasurementIsFixed) {
            measuredWidth = widthSize;
        }

        if (measuredHeight < contentHeightPix) {
            measuredHeight |= View.MEASURED_STATE_TOO_SMALL;
        }

        if (measuredWidth < contentWidthPix) {
            measuredWidth |= View.MEASURED_STATE_TOO_SMALL;
        }

        mDelegate.setMeasuredDimension(measuredWidth, measuredHeight);
    }

    /**
     * Notify the AwLayoutSizer that the size of the view has changed.
     * This should be called by the Android view system after onMeasure if the view's size has
     * changed.
     */
    public void onSizeChanged(int w, int h, int ow, int oh) {
        updateLayoutSettings();
    }

    /**
     * Notify the AwLayoutSizer that the layout pass requested via Delegate.requestLayout has
     * completed.
     * This should be called after onSizeChanged regardless of whether the size has changed or not.
     */
    public void onLayoutParamsChange() {
        updateLayoutSettings();
    }

    // This needs to be called every time either the physical size of the view is changed or layout
    // params are updated.
    private void updateLayoutSettings() {
        mDelegate.setForceZeroLayoutHeight(mDelegate.isLayoutParamsHeightWrapContent());
    }
}
