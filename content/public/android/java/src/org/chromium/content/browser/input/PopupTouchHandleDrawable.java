// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.SystemClock;
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
import org.chromium.content_public.browser.GestureStateListener;
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

    // The mirror values based on which the handles are inverted about X and Y axes.
    private boolean mMirrorHorizontal;
    private boolean mMirrorVertical;

    private float mAlpha;

    private final int[] mTempScreenCoords = new int[2];

    private int mOrientation = TouchHandleOrientation.UNDEFINED;

    // Length of the delay before fading in after the last page movement.
    private static final int MOVING_FADE_IN_DELAY_MS = 300;
    private static final int FADE_IN_DURATION_MS = 200;
    private Runnable mDeferredHandleFadeInRunnable;
    private long mFadeStartTime;
    private long mTemporarilyHiddenExpireTime;
    private boolean mVisible;
    private boolean mScrolling;
    private boolean mFocused;
    private boolean mTemporarilyHidden;

    // Gesture accounting for handle hiding while scrolling.
    private final GestureStateListener mGestureStateListener;

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

    private PopupTouchHandleDrawable(ContentViewCore contentViewCore) {
        super(contentViewCore.getContainerView().getContext());
        mContentViewCore = contentViewCore;
        mContainer = new PopupWindow(mContentViewCore.getWindowAndroid().getContext().get(),
                null, android.R.attr.textSelectHandleWindowStyle);
        mContainer.setSplitTouchEnabled(true);
        mContainer.setClippingEnabled(false);

        // The built-in PopupWindow animation causes jank when transitioning between
        // visibility states. We use a custom fade-in animation when necessary.
        mContainer.setAnimationStyle(0);

        // The SUB_PANEL window layout type improves z-ordering with respect to
        // other popup-based elements.
        setWindowLayoutType(mContainer, WindowManager.LayoutParams.TYPE_APPLICATION_SUB_PANEL);
        mContainer.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        mContainer.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);

        mAlpha = 1.f;
        mVisible = getVisibility() == VISIBLE;
        mFocused = mContentViewCore.getContainerView().hasWindowFocus();

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
        mGestureStateListener = new GestureStateListener() {
            @Override
            public void onScrollStarted(int scrollOffsetX, int scrollOffsetY) {
                setIsScrolling(true);
            }
            @Override
            public void onScrollEnded(int scrollOffsetX, int scrollOffsetY) {
                setIsScrolling(false);
            }
            @Override
            public void onFlingStartGesture(int vx, int vy, int scrollOffsetY, int scrollExtentY) {
                // Fling accounting is unreliable in WebView, as the embedder
                // can override onScroll() and suppress fling ticking. At best
                // we have to rely on the scroll offset changing to temporarily
                // and repeatedly keep the handles hidden.
                setIsScrolling(false);
            }
            @Override
            public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {
                temporarilyHide();
            }
            @Override
            public void onWindowFocusChanged(boolean hasWindowFocus) {
                setIsFocused(hasWindowFocus);
            }
            @Override
            public void onDestroyed() {
                destroy();
            }
        };
        mContentViewCore.addGestureStateListener(mGestureStateListener);
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

    private static Drawable getHandleDrawable(Context context, int orientation) {
        switch (orientation) {
            case TouchHandleOrientation.LEFT: {
                return HandleViewResources.getLeftHandleDrawable(context);
            }

            case TouchHandleOrientation.RIGHT: {
                return HandleViewResources.getRightHandleDrawable(context);
            }

            case TouchHandleOrientation.CENTER: {
                return HandleViewResources.getCenterHandleDrawable(context);
            }

            case TouchHandleOrientation.UNDEFINED:
            default:
                assert false;
                return HandleViewResources.getCenterHandleDrawable(context);
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
    private void setOrientation(int orientation, boolean mirrorVertical, boolean mirrorHorizontal) {
        assert (orientation >= TouchHandleOrientation.LEFT
                && orientation <= TouchHandleOrientation.UNDEFINED);

        final boolean orientationChanged = mOrientation != orientation;
        final boolean mirroringChanged =
                mMirrorHorizontal != mirrorHorizontal || mMirrorVertical != mirrorVertical;
        mOrientation = orientation;
        mMirrorHorizontal = mirrorHorizontal;
        mMirrorVertical = mirrorVertical;

        // Create new InvertedDrawable only if orientation has changed.
        // Otherwise, change the mirror values to scale canvas on draw() calls.
        if (orientationChanged) mDrawable = getHandleDrawable(getContext(), mOrientation);

        if (mDrawable != null) mDrawable.setAlpha((int) (255 * mAlpha));

        if (orientationChanged || mirroringChanged) scheduleInvalidate();
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

    private boolean isShowingAllowed() {
        return mVisible && mFocused && !mScrolling;
    }

    private void updateVisibility() {
        int newVisibility = isShowingAllowed() && !mTemporarilyHidden ? VISIBLE : INVISIBLE;

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

    private void setIsScrolling(boolean scrolling) {
        if (mScrolling == scrolling) return;
        mScrolling = scrolling;
        onVisibilityInputChanged();
    }

    private void setIsFocused(boolean focused) {
        if (mFocused == focused) return;
        mFocused = focused;
        onVisibilityInputChanged();
    }

    private void onVisibilityInputChanged() {
        if (!mContainer.isShowing()) return;
        if (isShowingAllowed()) {
            rescheduleFadeIn();
        } else {
            cancelFadeIn();
            updateVisibility();
        }
    }

    private void updateAlpha() {
        if (mAlpha == 1.f) return;
        long currentTimeMillis = AnimationUtils.currentAnimationTimeMillis();
        mAlpha = Math.min(1.f, (float) (currentTimeMillis - mFadeStartTime) / FADE_IN_DURATION_MS);
        mDrawable.setAlpha((int) (255 * mAlpha));
        scheduleInvalidate();
    }

    private void temporarilyHide() {
        if (!mContainer.isShowing()) return;
        mTemporarilyHidden = true;
        mTemporarilyHiddenExpireTime = SystemClock.uptimeMillis() + MOVING_FADE_IN_DELAY_MS;
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
        if (!isShowingAllowed()) return;
        if (mDeferredHandleFadeInRunnable == null) {
            mDeferredHandleFadeInRunnable = new Runnable() {
                @Override
                public void run() {
                    assert isShowingAllowed();
                    mTemporarilyHidden = false;
                    mTemporarilyHiddenExpireTime = 0;
                    beginFadeIn();
                }
            };
        }

        cancelFadeIn();
        long now = SystemClock.uptimeMillis();
        long delay = Math.max(0, mTemporarilyHiddenExpireTime - now);
        postOnAnimationDelayed(mDeferredHandleFadeInRunnable, delay);
    }

    private void cancelFadeIn() {
        if (mDeferredHandleFadeInRunnable == null) return;
        removeCallbacks(mDeferredHandleFadeInRunnable);
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
        final boolean needsMirror = mMirrorHorizontal || mMirrorVertical;
        if (needsMirror) {
            c.save();
            float scaleX = mMirrorHorizontal ? -1.f : 1.f;
            float scaleY = mMirrorVertical ? -1.f : 1.f;
            c.scale(scaleX, scaleY, getWidth() / 2.0f, getHeight() / 2.0f);
        }
        updateAlpha();
        mDrawable.setBounds(0, 0, getRight() - getLeft(), getBottom() - getTop());
        mDrawable.draw(c);
        if (needsMirror) c.restore();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        hide();
    }

    @CalledByNative
    private void destroy() {
        if (mContentViewCore == null) return;
        hide();
        mContentViewCore.removeGestureStateListener(mGestureStateListener);
        mContentViewCore.removeContainerViewObserver(mParentViewObserver);
        mContentViewCore = null;
    }

    @CalledByNative
    private void show() {
        if (mContentViewCore == null) return;
        if (mContainer.isShowing()) return;

        // While hidden, the parent position may have become stale. It must be updated before
        // checking isPositionVisible().
        updateParentPosition(mParentPositionObserver.getPositionX(),
                mParentPositionObserver.getPositionY());
        mParentPositionObserver.addListener(mParentPositionListener);
        mContainer.setContentView(this);
        try {
            mContainer.showAtLocation(mContentViewCore.getContainerView(), 0,
                    getContainerPositionX(), getContainerPositionY());
        } catch (WindowManager.BadTokenException e) {
            hide();
        }
    }

    @CalledByNative
    private void hide() {
        mTemporarilyHidden = false;
        mTemporarilyHiddenExpireTime = 0;
        mAlpha = 1.0f;
        cancelFadeIn();
        if (mContainer.isShowing()) mContainer.dismiss();
        mParentPositionObserver.clearListener();
    }

    @CalledByNative
    private void setOrigin(float originX, float originY) {
        if (mPositionX == originX && mPositionY == originY) return;
        mPositionX = (int) originX;
        mPositionY = (int) originY;
        if (getVisibility() == VISIBLE) scheduleInvalidate();
    }

    @CalledByNative
    private void setVisible(boolean visible) {
        if (mVisible == visible) return;
        mVisible = visible;
        onVisibilityInputChanged();
    }

    @CalledByNative
    private int getPositionX() {
        return mPositionX;
    }

    @CalledByNative
    private float getHandleHorizontalPaddingRatio() {
        return HandleViewResources.getHandleHorizontalPaddingRatio();
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
