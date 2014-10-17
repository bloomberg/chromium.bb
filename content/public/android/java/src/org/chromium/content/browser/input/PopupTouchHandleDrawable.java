// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;
import android.view.View;
import android.view.animation.AnimationUtils;
import android.widget.PopupWindow;

import org.chromium.base.ApiCompatibilityUtils;
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

    // Length of the delay before fading in after the last page movement.
    private static final int FADE_IN_DELAY_MS = 300;
    private static final int FADE_IN_DURATION_MS = 200;
    private Runnable mDeferredHandleFadeInRunnable;
    private long mFadeStartTime;
    private boolean mVisible;
    private boolean mTemporarilyHidden;

    // Deferred runnable to avoid invalidating outside of frame dispatch,
    // in turn avoiding issues with sync barrier insertion.
    private Runnable mInvalidationRunnable;
    private boolean mHasPendingInvalidate;

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

        /**
         * @return Whether the associated content is actively scrolling.
         */
        boolean isScrollInProgress();
    }

    public PopupTouchHandleDrawable(PopupTouchHandleDrawableDelegate delegate) {
        super(delegate.getParent().getContext());
        mContext = delegate.getParent().getContext();
        mDelegate = new WeakReference<PopupTouchHandleDrawableDelegate>(delegate);
        mContainer = new PopupWindow(mContext, null, android.R.attr.textSelectHandleWindowStyle);
        mContainer.setSplitTouchEnabled(true);
        mContainer.setClippingEnabled(false);
        mContainer.setAnimationStyle(0);
        mAlpha = 1.f;
        mVisible = getVisibility() == VISIBLE;
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
        if (hadValidOrientation) setFocus(oldAdjustedPositionX, oldAdjustedPositionY);
        mDrawable.setAlpha((int) (255 * mAlpha));
        scheduleInvalidate();
    }

    private void updateParentPosition(int parentPositionX, int parentPositionY) {
        if (mParentPositionX == parentPositionX && mParentPositionY == parentPositionY) return;
        mParentPositionX = parentPositionX;
        mParentPositionY = parentPositionY;
        temporarilyHide();
    }

    private int getContainerPositionX() {
        return mParentPositionX + mPositionX;
    }

    private int getContainerPositionY() {
        return mParentPositionY + mPositionY;
    }

    private void updatePosition() {
        mContainer.update(getContainerPositionX(), getContainerPositionY(),
                getRight() - getLeft(), getBottom() - getTop());
    }

    private void updateVisibility() {
        boolean visible = mVisible && !mTemporarilyHidden;
        setVisibility(visible ? VISIBLE : INVISIBLE);
    }

     private void updateAlpha() {
        if (mAlpha == 1.f) return;
        long currentTimeMillis = AnimationUtils.currentAnimationTimeMillis();
        mAlpha = Math.min(1.f, (float) (currentTimeMillis - mFadeStartTime) / FADE_IN_DURATION_MS);
        mDrawable.setAlpha((int) (255 * mAlpha));
        scheduleInvalidate();
    }

    private void temporarilyHide() {
        mTemporarilyHidden = true;
        updateVisibility();
        rescheduleFadeIn();
    }

    private void doInvalidate() {
        updatePosition();
        updateVisibility();
        invalidate();
    }

    private void scheduleInvalidate() {
        if (mInvalidationRunnable == null) {
            mInvalidationRunnable = new Runnable() {
                @Override
                public void run() {
                    mHasPendingInvalidate = false;
                    doInvalidate();
                }
            };
        }

        if (mHasPendingInvalidate) return;
        mHasPendingInvalidate = true;
        ApiCompatibilityUtils.postOnAnimation(this, mInvalidationRunnable);
    }

    private void rescheduleFadeIn() {
        if (mDeferredHandleFadeInRunnable == null) {
            mDeferredHandleFadeInRunnable = new Runnable() {
                @Override
                public void run() {
                    if (isScrollInProgress()) {
                        rescheduleFadeIn();
                        return;
                    }
                    mTemporarilyHidden = false;
                    beginFadeIn();
                }
            };
        }

        removeCallbacks(mDeferredHandleFadeInRunnable);
        ApiCompatibilityUtils.postOnAnimationDelayed(
                this, mDeferredHandleFadeInRunnable, FADE_IN_DELAY_MS);
    }

    private void beginFadeIn() {
        if (getVisibility() == VISIBLE) return;
        mAlpha = 0.f;
        mFadeStartTime = AnimationUtils.currentAnimationTimeMillis();
        doInvalidate();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mDrawable == null) {
            setMeasuredDimension(0, 0);
            return;
        }
        setMeasuredDimension(mDrawable.getIntrinsicWidth(), mDrawable.getIntrinsicHeight());
    }

    @Override
    protected void onDraw(Canvas c) {
        if (mDrawable == null) return;
        updateAlpha();
        mDrawable.setBounds(0, 0, getRight() - getLeft(), getBottom() - getTop());
        mDrawable.draw(c);
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

    private boolean isScrollInProgress() {
        final PopupTouchHandleDrawableDelegate delegate = mDelegate.get();
        if (delegate == null) {
            hide();
            return false;
        }

        return delegate.isScrollInProgress();
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
        mTemporarilyHidden = false;
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
        // Ignore opacity updates from the caller as they are not compatible
        // with the custom fade animation.
    }

    @CalledByNative
    private void setFocus(float focusX, float focusY) {
        int x = (int) focusX - Math.round(mHotspotX);
        int y = (int) focusY - Math.round(mHotspotY);
        if (mPositionX == x && mPositionY == y) return;
        mPositionX = x;
        mPositionY = y;
        if (isScrollInProgress()) {
            temporarilyHide();
        } else {
            scheduleInvalidate();
        }
    }

    @CalledByNative
    private void setVisible(boolean visible) {
        mVisible = visible;
        int visibility = visible ? VISIBLE : INVISIBLE;
        if (getVisibility() == visibility) return;
        scheduleInvalidate();
    }

    @CalledByNative
    private boolean intersectsWith(float x, float y, float width, float height) {
        if (mDrawable == null) return false;
        final int drawableWidth = mDrawable.getIntrinsicWidth();
        final int drawableHeight = mDrawable.getIntrinsicHeight();
        return !(x >= mPositionX + drawableWidth
                || y >= mPositionY + drawableHeight
                || x + width <= mPositionX
                || y + height <= mPositionY);
    }
}
