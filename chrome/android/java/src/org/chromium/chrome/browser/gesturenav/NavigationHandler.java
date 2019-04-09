// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.support.annotation.IntDef;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.tab.Tab;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Handles history overscroll navigation controlling the underlying UI widget.
 */
public class NavigationHandler {
    @IntDef({GestureState.NONE, GestureState.STARTED, GestureState.DRAGGED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface GestureState {
        int NONE = 0;
        int STARTED = 1;
        int DRAGGED = 2;
        int GLOW = 3;
    }

    private final Context mContext;

    private Supplier<Tab> mTabProvider;

    private ViewGroup mParentView;
    private GestureDetector mGestureDetector;
    private @GestureState int mState = GestureState.NONE;

    // Frame layout where the main logic turning the gesture into corresponding UI resides.
    private SideSlideLayout mSideSlideLayout;

    // Async runnable for ending the refresh animation after the page first
    // loads a frame. This is used to provide a reasonable minimum animation time.
    private Runnable mStopNavigatingRunnable;

    // Handles removing the layout from the view hierarchy.  This is posted to ensure
    // it does not conflict with pending Android draws.
    private Runnable mDetachLayoutRunnable;

    public NavigationHandler(Context context, Supplier<Tab> tabProvider) {
        mContext = context;
        mTabProvider = tabProvider;
    }

    /**
     * Sets the view to which a widget view is added.
     * @param view Parent view to contain the navigation UI view.
     */
    public void setParentView(ViewGroup view) {
        if (view == null && mParentView != null) detachLayoutIfNecessary();
        mParentView = view;
    }

    private void createLayout() {
        mSideSlideLayout = new SideSlideLayout(mContext);
        mSideSlideLayout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mSideSlideLayout.setOnNavigationListener((isForward) -> {
            Tab tab = mTabProvider.get();
            if (isForward) {
                tab.goForward();
            } else {
                if (canNavigate(/* forward= */ false)) {
                    tab.goBack();
                } else {
                    tab.getActivity().onBackPressed();
                }
            }
            cancelStopNavigatingRunnable();
            mSideSlideLayout.post(getStopNavigatingRunnable());
        });

        mSideSlideLayout.setOnResetListener(() -> {
            if (mDetachLayoutRunnable != null) return;
            mDetachLayoutRunnable = () -> {
                mDetachLayoutRunnable = null;
                detachLayoutIfNecessary();
            };
            mSideSlideLayout.post(mDetachLayoutRunnable);
        });
    }

    private boolean canNavigate(boolean forward) {
        Tab tab = mTabProvider.get();
        if (tab == null) return false;
        return forward ? tab.canGoForward() : tab.canGoBack();
    }

    /**
     * @see View#onTouchEvent(MotionEvent)
     */
    void onTouchEvent(MotionEvent e) {
        if (e.getAction() == MotionEvent.ACTION_UP) {
            if (mState == GestureState.DRAGGED) mSideSlideLayout.release(true);
        }
    }

    /**
     * @see GestureDetector#SimpleOnGestureListener#onDown(MotionEvent)
     */
    boolean onDown(MotionEvent event) {
        mState = GestureState.STARTED;
        return true;
    }

    /**
     * @see GestureDetector#SimpleOnGestureListener#onScroll(MotionEvent, MotionEvent, float, float)
     */
    boolean onScroll(float distanceX, float distanceY) {
        // onScroll needs handling only after the state moves away from none state.
        if (mState == GestureState.NONE) return true;

        if (mState == GestureState.STARTED) {
            if (Math.abs(distanceX) > Math.abs(distanceY)) {
                boolean forward = distanceX > 0;
                boolean navigable = canNavigate(forward);
                if (navigable || !forward) {
                    start(forward, !navigable);
                    mState = GestureState.DRAGGED;
                }
            }
            if (mState != GestureState.DRAGGED) mState = GestureState.NONE;
        }
        if (mState == GestureState.DRAGGED) pull(-distanceX);
        return true;
    }

    /**
     * Initiates navigation UI widget on the screen.
     * @param isForward {@code true} if started for forward navigation.
     * @param enableCloseIndicator Whether 'close chrome' indicator should be
     *     enabled when the condition is met.
     */
    public void start(boolean isForward, boolean enableCloseIndicator) {
        if (mSideSlideLayout == null) createLayout();
        mSideSlideLayout.setEnabled(true);
        mSideSlideLayout.setDirection(isForward);
        mSideSlideLayout.setEnableCloseIndicator(enableCloseIndicator);
        attachLayoutIfNecessary();
        mSideSlideLayout.start();
    }

    /**
     * Signals a pull update.
     * @param delta The change in horizontal pull distance (positive if toward right,
     *         negative if left).
     */
    public void pull(float delta) {
        if (mSideSlideLayout != null) mSideSlideLayout.pull(delta);
    }

    /**
     * @return {@code true} if navigation is not in operation.
     */
    boolean isStopped() {
        return mState == GestureState.NONE;
    }

    /**
     * Release the active pull. If no pull has started, the release will be ignored.
     * If the pull was sufficiently large, the navigation sequence will be initiated.
     * @param allowNav Whether to allow a sufficiently large pull to trigger
     *         the navigation action and animation sequence.
     */
    public void release(boolean allowNav) {
        if (mSideSlideLayout != null) {
            cancelStopNavigatingRunnable();
            mSideSlideLayout.release(allowNav);
        }
    }

    /**
     * Reset navigation UI in action.
     */
    public void reset() {
        if (mSideSlideLayout != null) {
            cancelStopNavigatingRunnable();
            mSideSlideLayout.reset();
        }
        mState = GestureState.NONE;
    }

    private void cancelStopNavigatingRunnable() {
        if (mStopNavigatingRunnable != null) {
            mSideSlideLayout.removeCallbacks(mStopNavigatingRunnable);
            mStopNavigatingRunnable = null;
        }
    }

    private void cancelDetachLayoutRunnable() {
        if (mDetachLayoutRunnable != null) {
            mSideSlideLayout.removeCallbacks(mDetachLayoutRunnable);
            mDetachLayoutRunnable = null;
        }
    }

    private Runnable getStopNavigatingRunnable() {
        if (mStopNavigatingRunnable == null) {
            mStopNavigatingRunnable = () -> mSideSlideLayout.stopNavigating();
        }
        return mStopNavigatingRunnable;
    }

    private void attachLayoutIfNecessary() {
        // The animation view is attached/detached on-demand to minimize overlap
        // with composited SurfaceView content.
        cancelDetachLayoutRunnable();
        if (mSideSlideLayout.getParent() == null) {
            mParentView.addView(mSideSlideLayout);
        }
    }

    private void detachLayoutIfNecessary() {
        if (mSideSlideLayout == null) return;
        cancelDetachLayoutRunnable();
        if (mSideSlideLayout.getParent() != null) {
            mParentView.removeView(mSideSlideLayout);
        }
    }
}
