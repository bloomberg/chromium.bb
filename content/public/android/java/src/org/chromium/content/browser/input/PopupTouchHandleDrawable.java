// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;
import android.view.View;
import android.widget.PopupWindow;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.content.browser.PositionObserver;

import java.lang.ref.WeakReference;

/**
 * View that displays a selection or insertion handle for text editing.
 *
 * While a HandleView is logically a child of some other view, it does not exist in that View's
 * hierarchy.
 *
 */
@JNINamespace("content")
public class PopupTouchHandleDrawable extends View {
    private Drawable mDrawable;
    private final PopupWindow mContainer;
    private final Context mContext;
    private final PositionObserver.Listener mParentPositionListener;

    // The weak delegate reference allows the PopupTouchHandleDrawable to be owned by a native
    // object that might have a different lifetime (or a cyclic lifetime) with respect to the
    // delegate, allowing garbage collection of any Java references.
    private final WeakReference<PopupTouchHandleDrawableDelegate> mDelegate;

    // The observer reference will only be non-null while it is attached to mParentPositionListener.
    private PositionObserver mParentPositionObserver;

    // The position of the handle relative to the parent view.
    private int mPositionX;
    private int mPositionY;

    // The position of the parent relative to the application's root view.
    private int mParentPositionX;
    private int mParentPositionY;

    // The offset from this handles position to the "tip" of the handle.
    private float mHotspotX;
    private float mHotspotY;

    private float mAlpha;

    private final int[] mTempScreenCoords = new int[2];

    static final int LEFT = 0;
    static final int CENTER = 1;
    static final int RIGHT = 2;
    private int mOrientation = -1;

    /**
     * Provides additional interaction behaviors necessary for handle
     * manipulation and interaction.
     */
    public interface PopupTouchHandleDrawableDelegate {
        /**
         * @return The parent View of the PopupWindow.
         */
        View getParent();

        /**
         * @return A position observer for the parent View, used to keep the
         *         absolutely positioned PopupWindow in-sync with the parent.
         */
        PositionObserver getParentPositionObserver();

        /**
         * Should route MotionEvents to the appropriate logic layer for
         * performing handle manipulation.
         */
        boolean onTouchHandleEvent(MotionEvent ev);
    }

    public PopupTouchHandleDrawable(PopupTouchHandleDrawableDelegate delegate) {
        super(delegate.getParent().getContext());
        mContext = delegate.getParent().getContext();
        mDelegate = new WeakReference<PopupTouchHandleDrawableDelegate>(delegate);
        mContainer = new PopupWindow(mContext, null, android.R.attr.textSelectHandleWindowStyle);
        mContainer.setSplitTouchEnabled(true);
        mContainer.setClippingEnabled(false);
        mAlpha = 1.f;
        mParentPositionListener = new PositionObserver.Listener() {
            @Override
            public void onPositionChanged(int x, int y) {
                updateParentPosition(x, y);
            }
        };
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        final PopupTouchHandleDrawableDelegate delegate = mDelegate.get();
        if (delegate == null) {
            // If the delegate is gone, we should immediately dispose of the popup.
            hide();
            return false;
        }

        // Convert from PopupWindow local coordinates to
        // parent view local coordinates prior to forwarding.
        delegate.getParent().getLocationOnScreen(mTempScreenCoords);
        final float offsetX = event.getRawX() - event.getX() - mTempScreenCoords[0];
        final float offsetY = event.getRawY() - event.getY() - mTempScreenCoords[1];
        final MotionEvent offsetEvent = MotionEvent.obtainNoHistory(event);
        offsetEvent.offsetLocation(offsetX, offsetY);
        final boolean handled = delegate.onTouchHandleEvent(offsetEvent);
        offsetEvent.recycle();
        return handled;
    }

    private void setOrientation(int orientation) {
        assert orientation >= LEFT && orientation <= RIGHT;
        if (mOrientation == orientation) return;

        final boolean hadValidOrientation = mOrientation != -1;
        mOrientation = orientation;

        final int oldAdjustedPositionX = getAdjustedPositionX();
        final int oldAdjustedPositionY = getAdjustedPositionY();

        switch (orientation) {
            case LEFT: {
                mDrawable = HandleViewResources.getLeftHandleDrawable(mContext);
                mHotspotX = (mDrawable.getIntrinsicWidth() * 3) / 4f;
                break;
            }

            case RIGHT: {
                mDrawable = HandleViewResources.getRightHandleDrawable(mContext);
                mHotspotX = mDrawable.getIntrinsicWidth() / 4f;
                break;
            }

            case CENTER:
            default: {
                mDrawable = HandleViewResources.getCenterHandleDrawable(mContext);
                mHotspotX = mDrawable.getIntrinsicWidth() / 2f;
                break;
            }
        }
        mHotspotY = 0;

        // Force handle repositioning to accommodate the new orientation's hotspot.
        if (hadValidOrientation) positionAt(oldAdjustedPositionX, oldAdjustedPositionY);
        mDrawable.setAlpha((int) (255 * mAlpha));

        invalidate();
    }

    private void updateParentPosition(int parentPositionX, int parentPositionY) {
        mParentPositionX = parentPositionX;
        mParentPositionY = parentPositionY;
        onPositionChanged();
    }

    private int getContainerPositionX() {
        return mParentPositionX + mPositionX;
    }

    private int getContainerPositionY() {
        return mParentPositionY + mPositionY;
    }

    private void onPositionChanged() {
        mContainer.update(getContainerPositionX(), getContainerPositionY(),
                getRight() - getLeft(), getBottom() - getTop());
    }

    // x and y are in physical pixels.
    private void moveTo(int x, int y) {
        mPositionX = x;
        mPositionY = y;
        onPositionChanged();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mDrawable == null) {
            setMeasuredDimension(0, 0);
            return;
        }
        setMeasuredDimension(mDrawable.getIntrinsicWidth(),
                mDrawable.getIntrinsicHeight());
    }

    @Override
    protected void onDraw(Canvas c) {
        if (mDrawable == null) return;
        mDrawable.setBounds(0, 0, getRight() - getLeft(), getBottom() - getTop());
        mDrawable.draw(c);
    }

    // x and y are in physical pixels.
    private void positionAt(int x, int y) {
        moveTo(x - Math.round(mHotspotX), y - Math.round(mHotspotY));
    }

    // Returns the x coordinate of the position that the handle appears to be pointing to relative
    // to the handles "parent" view.
    private int getAdjustedPositionX() {
        return mPositionX + Math.round(mHotspotX);
    }

    // Returns the y coordinate of the position that the handle appears to be pointing to relative
    // to the handles "parent" view.
    private int getAdjustedPositionY() {
        return mPositionY + Math.round(mHotspotY);
    }

    @CalledByNative
    private void show() {
        if (mContainer.isShowing()) return;

        final PopupTouchHandleDrawableDelegate delegate = mDelegate.get();
        if (delegate == null) {
            hide();
            return;
        }

        mParentPositionObserver = delegate.getParentPositionObserver();
        assert mParentPositionObserver != null;

        // While hidden, the parent position may have become stale. It must be updated before
        // checking isPositionVisible().
        updateParentPosition(mParentPositionObserver.getPositionX(),
                mParentPositionObserver.getPositionY());
        mParentPositionObserver.addListener(mParentPositionListener);
        mContainer.setContentView(this);
        mContainer.showAtLocation(delegate.getParent(), 0,
                getContainerPositionX(), getContainerPositionY());
    }

    @CalledByNative
    private void hide() {
        mContainer.dismiss();
        if (mParentPositionObserver != null) {
            mParentPositionObserver.removeListener(mParentPositionListener);
            // Clear the strong reference to allow garbage collection.
            mParentPositionObserver = null;
        }
    }

    @CalledByNative
    private void setRightOrientation() {
        setOrientation(RIGHT);
    }

    @CalledByNative
    private void setLeftOrientation() {
        setOrientation(LEFT);
    }

    @CalledByNative
    private void setCenterOrientation() {
        setOrientation(CENTER);
    }

    @CalledByNative
    private void setOpacity(float alpha) {
        if (mAlpha == alpha) return;
        mAlpha = alpha;
        if (mDrawable != null) mDrawable.setAlpha((int) (255 * mAlpha));
    }

    @CalledByNative
    private void setFocus(float x, float y) {
        positionAt((int) x, (int) y);
    }

    @CalledByNative
    private void setVisible(boolean visible) {
        setVisibility(visible ? VISIBLE : INVISIBLE);
    }

    @CalledByNative
    private boolean containsPoint(float x, float y) {
        if (mDrawable == null) return false;
        final int width = mDrawable.getIntrinsicWidth();
        final int height = mDrawable.getIntrinsicHeight();
        return x > mPositionX && x < (mPositionX + width)
                && y > mPositionY && y < (mPositionY + height);
    }
}
