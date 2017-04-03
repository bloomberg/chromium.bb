// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.textbubble;

import android.content.Context;
import android.graphics.Rect;
import android.support.annotation.StringRes;
import android.view.View;
import android.view.View.OnLayoutChangeListener;

import org.chromium.base.ApiCompatibilityUtils;

/**
 * A helper class that anchors a {@link TextBubble} to a particular {@link View}.  The bubble will
 * listen to layout events on the {@link View} and update accordingly.
 */
public class ViewAnchoredTextBubble extends TextBubble implements OnLayoutChangeListener {
    private final int[] mCachedScreenCoordinates = new int[2];
    private final Rect mAnchorRect = new Rect();
    private final Rect mInsetRect = new Rect();
    private final View mAnchorView;

    /**
     * Creates an instance of a {@link ViewAnchoredTextBubble}.
     * @param context    Context to draw resources from.
     * @param anchorView The {@link View} to anchor to.
     * @param stringId The id of the string resource for the text that should be shown.
     */
    public ViewAnchoredTextBubble(Context context, View anchorView, @StringRes int stringId) {
        super(context, anchorView.getRootView(), stringId);
        mAnchorView = anchorView;
    }

    /**
     * Specifies the inset values in pixels that determine how to shrink the {@link View} bounds
     * when creating the anchor {@link Rect}.
     */
    public void setInsetPx(int left, int top, int right, int bottom) {
        mInsetRect.set(left, top, right, bottom);
        refreshAnchorBounds();
    }

    // TextBubble implementation.
    @Override
    public void show() {
        mAnchorView.addOnLayoutChangeListener(this);
        refreshAnchorBounds();
        super.show();
    }

    @Override
    public void dismiss() {
        super.dismiss();
        mAnchorView.removeOnLayoutChangeListener(this);
    }

    // OnLayoutChangeListener implementation.
    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if (!mAnchorView.isShown()) {
            dismiss();
            return;
        }

        refreshAnchorBounds();
    }

    private void refreshAnchorBounds() {
        mAnchorView.getLocationOnScreen(mCachedScreenCoordinates);
        mAnchorRect.left = mCachedScreenCoordinates[0] + mInsetRect.left;
        mAnchorRect.top = mCachedScreenCoordinates[1] + mInsetRect.top;
        mAnchorRect.right = mAnchorRect.left + mAnchorView.getWidth() - mInsetRect.right;
        mAnchorRect.bottom = mAnchorRect.top + mAnchorView.getHeight() - mInsetRect.bottom;

        // Account for the padding.
        boolean isRtl = ApiCompatibilityUtils.isLayoutRtl(mAnchorView);
        mAnchorRect.left += isRtl ? ApiCompatibilityUtils.getPaddingEnd(mAnchorView)
                                  : ApiCompatibilityUtils.getPaddingStart(mAnchorView);
        mAnchorRect.right -= isRtl ? ApiCompatibilityUtils.getPaddingStart(mAnchorView)
                                   : ApiCompatibilityUtils.getPaddingEnd(mAnchorView);
        mAnchorRect.top += mAnchorView.getPaddingTop();
        mAnchorRect.bottom -= mAnchorView.getPaddingBottom();

        // Make sure we still have a valid Rect after applying the inset.
        mAnchorRect.right = Math.max(mAnchorRect.left, mAnchorRect.right);
        mAnchorRect.bottom = Math.max(mAnchorRect.top, mAnchorRect.bottom);

        setAnchorRect(mAnchorRect);
    }
}