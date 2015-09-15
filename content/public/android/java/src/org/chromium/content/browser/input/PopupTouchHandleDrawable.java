// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.annotation.SuppressLint;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.animation.AnimationUtils;
import android.widget.PopupWindow;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.ContainerViewObserver;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.PositionObserver;
import org.chromium.content.browser.ViewPositionObserver;
import org.chromium.ui.touch_selection.TouchHandleOrientation;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * View that displays a selection or insertion handle for text editing.
 *
 * While a HandleView is logically a child of some other view, it does not exist in that View's
 * hierarchy.
 *
 */
@JNINamespace("content")
public class PopupTouchHandleDrawable extends View {
    private final PopupWindow mContainer;
    private final PositionObserver.Listener mParentPositionListener;
    private final ContainerViewObserver mParentViewObserver;
    private ContentViewCore mContentViewCore;
    private PositionObserver mParentPositionObserver;
    private Drawable mDrawable;

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

    private int mOrientation = TouchHandleOrientation.UNDEFINED;

    // Length of the delay before fading in after the last page movement.
    private static final int FADE_IN_DELAY_MS = 300;
    private static final int FADE_IN_DURATION_MS = 200;
    private Runnable mDeferredHandleFadeInRunnable;
    private long mFadeStartTime;
    private boolean mVisible;
    private boolean mTemporarilyHidden;

    // There are no guarantees that the side effects of setting the position of
    // the PopupWindow and the visibility of its content View will be realized
    // in the same frame. Thus, to ensure the PopupWindow is seen in the right
    // location, when the PopupWindow reappears we delay the visibility update
    // by one frame after setting the position.
    private boolean mDelayVisibilityUpdateWAR;

    // Deferred runnable to avoid invalidating outside of frame dispatch,
    // in turn avoiding issues with sync barrier insertion.
    private Runnable mInvalidationRunnable;
    private boolean mHasPendingInvalidate;

    public PopupTouchHandleDrawable(ContentViewCore contentViewCore) {
        super(contentViewCore.getContainerView().getContext());
        mContentViewCore = contentViewCore;
        mContainer =
                new PopupWindow(getContext(), null, android.R.attr.textSelectHandleWindowStyle);
        mContainer.setSplitTouchEnabled(true);
        mContainer.setClippingEnabled(false);
        mContainer.setAnimationStyle(0);
        mContainer.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        mContainer.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        setWindowLayoutType(mContainer, WindowManager.LayoutParams.TYPE_APPLICATION_SUB_PANEL);
        mAlpha = 1.f;
        mVisible = getVisibility() == VISIBLE;
        mParentPositionObserver = new ViewPositionObserver(mContentViewCore.getContainerView());
        mParentPositionListener = new PositionObserver.Listener() {
            @Override
            public void onPositionChanged(int x, int y) {
                updateParentPosition(x, y);
            }
        };
        mParentViewObserver = new ContainerViewObserver() {
            @Override
            public void onContainerViewChanged(ViewGroup newContainerView) {
                // If the parent View ever changes, the parent position observer
                // must be updated accordingly.
                mParentPositionObserver.clearListener();
                mParentPositionObserver = new ViewPositionObserver(newContainerView);
                if (mContainer.isShowing()) {
                    mParentPositionObserver.addListener(mParentPositionListener);
                }
            }
        };
        mContentViewCore.addContainerViewObserver(mParentViewObserver);
    }

    private static void setWindowLayoutType(PopupWindow window, int layoutType) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            window.setWindowLayoutType(layoutType);
            return;
        }

        try {
            Method setWindowLayoutTypeMethod =
                    PopupWindow.class.getMethod("setWindowLayoutType", int.class);
            setWindowLayoutTypeMethod.invoke(window, layoutType);
        } catch (NoSuchMethodException | IllegalAccessException | InvocationTargetException
                | RuntimeException e) {
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mContentViewCore == null) return false;
        // Convert from PopupWindow local coordinates to
        // parent view local coordinates prior to forwarding.
        mContentViewCore.getContainerView().getLocationOnScreen(mTempScreenCoords);
        final float offsetX = event.getRawX() - event.getX() - mTempScreenCoords[0];
        final float offsetY = event.getRawY() - event.getY() - mTempScreenCoords[1];
        final MotionEvent offsetEvent = MotionEvent.obtainNoHistory(event);
        offsetEvent.offsetLocation(offsetX, offsetY);
        final boolean handled = mContentViewCore.onTouchHandleEvent(offsetEvent);
        offsetEvent.recycle();
        return handled;
    }

    @CalledByNative
    private static PopupTouchHandleDrawable create(ContentViewCore contentViewCore) {
        return new PopupTouchHandleDrawable(contentViewCore);
    }

    @CalledByNative
    private void setOrientation(int orientation) {
        assert (orientation >= TouchHandleOrientation.LEFT
                && orientation <= TouchHandleOrientation.UNDEFINED);
        if (mOrientation == orientation) return;

        final boolean hadValidOrientation = mOrientation != TouchHandleOrientation.UNDEFINED;
        mOrientation = orientation;

        final int oldAdjustedPositionX = getAdjustedPositionX();
        final int oldAdjustedPositionY = getAdjustedPositionY();

        switch (orientation) {
            case TouchHandleOrientation.LEFT: {
                mDrawable = HandleViewResources.getLeftHandleDrawable(getContext());
                mHotspotX = (mDrawable.getIntrinsicWidth() * 3) / 4f;
                break;
            }

            case TouchHandleOrientation.RIGHT: {
                mDrawable = HandleViewResources.getRightHandleDrawable(getContext());
                mHotspotX = mDrawable.getIntrinsicWidth() / 4f;
                break;
            }

            case TouchHandleOrientation.CENTER: {
                mDrawable = HandleViewResources.getCenterHandleDrawable(getContext());
                mHotspotX = mDrawable.getIntrinsicWidth() / 2f;
                break;
            }

            case TouchHandleOrientation.UNDEFINED:
            default:
                break;
        }
        mHotspotY = 0;

        // Force handle repositioning to accommodate the new orientation's hotspot.
        if (hadValidOrientation) setFocus(oldAdjustedPositionX, oldAdjustedPositionY);
        if (mDrawable != null) mDrawable.setAlpha((int) (255 * mAlpha));
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
        int newVisibility = mVisible && !mTemporarilyHidden ? VISIBLE : INVISIBLE;

        // When regaining visibility, delay the visibility update by one frame
        // to ensure the PopupWindow has first been positioned properly.
        if (newVisibility == VISIBLE && getVisibility() != VISIBLE) {
            if (!mDelayVisibilityUpdateWAR) {
                mDelayVisibilityUpdateWAR = true;
                scheduleInvalidate();
                return;
            }
        }
        mDelayVisibilityUpdateWAR = false;

        setVisibility(newVisibility);
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
        if (!mContainer.isShowing()) return;
        updateVisibility();
        updatePosition();
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
        postOnAnimation(mInvalidationRunnable);
    }

    private void rescheduleFadeIn() {
        if (mDeferredHandleFadeInRunnable == null) {
            mDeferredHandleFadeInRunnable = new Runnable() {
                @Override
                public void run() {
                    if (mContentViewCore.isScrollInProgress()) {
                        rescheduleFadeIn();
                        return;
                    }
                    mTemporarilyHidden = false;
                    beginFadeIn();
                }
            };
        }

        removeCallbacks(mDeferredHandleFadeInRunnable);
        postOnAnimationDelayed(mDeferredHandleFadeInRunnable, FADE_IN_DELAY_MS);
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

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        hide();
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
    private void destroy() {
        hide();
        mContentViewCore.removeContainerViewObserver(mParentViewObserver);
        mContentViewCore = null;
    }

    @CalledByNative
    private void show() {
        if (mContainer.isShowing()) return;

        // While hidden, the parent position may have become stale. It must be updated before
        // checking isPositionVisible().
        updateParentPosition(mParentPositionObserver.getPositionX(),
                mParentPositionObserver.getPositionY());
        mParentPositionObserver.addListener(mParentPositionListener);
        mContainer.setContentView(this);
        mContainer.showAtLocation(mContentViewCore.getContainerView(), 0,
                getContainerPositionX(), getContainerPositionY());
    }

    @CalledByNative
    private void hide() {
        mTemporarilyHidden = false;
        mAlpha = 1.0f;
        if (mDeferredHandleFadeInRunnable != null) removeCallbacks(mDeferredHandleFadeInRunnable);
        if (mContainer.isShowing()) mContainer.dismiss();
        mParentPositionObserver.clearListener();
    }

    @CalledByNative
    private void setFocus(float focusX, float focusY) {
        int x = (int) focusX - Math.round(mHotspotX);
        int y = (int) focusY - Math.round(mHotspotY);
        if (mPositionX == x && mPositionY == y) return;
        mPositionX = x;
        mPositionY = y;
        if (mContentViewCore.isScrollInProgress()) {
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
    private int getPositionX() {
        return mPositionX;
    }

    @CalledByNative
    private int getPositionY() {
        return mPositionY;
    }

    @CalledByNative
    private int getVisibleWidth() {
        if (mDrawable == null) return 0;
        return mDrawable.getIntrinsicWidth();
    }

    @CalledByNative
    private int getVisibleHeight() {
        if (mDrawable == null) return 0;
        return mDrawable.getIntrinsicHeight();
    }
}
