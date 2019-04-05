// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabObserver;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;

/**
 * FrameLayout that supports side-wise slide gesture for history navigation. Inheriting
 * class may need to override {@link #wasLastSideSwipeGestureConsumed()} if
 * {@link #onTouchEvent} cannot be relied upon to know whether the side-swipe related
 * event was handled. Namely {@link android.support.v7.widget.RecyclerView}) always
 * claims to handle touch events.
 */
public class HistoryNavigationLayout extends FrameLayout {
    private final ActivityTabObserver mTabObserver;
    private final ActivityTabProvider mTabProvider;
    private final GestureDetector mDetector;
    private final NavigationHandler mNavigationHandler;

    public HistoryNavigationLayout(Context context) {
        this(context, null);
    }

    public HistoryNavigationLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.OVERSCROLL_HISTORY_NAVIGATION)
                && context instanceof ChromeActivity) {
            mDetector = new GestureDetector(getContext(), new SideNavGestureListener());
            mTabProvider = ((ChromeActivity) context).getActivityTabProvider();
            mNavigationHandler = new NavigationHandler(context, mTabProvider);
            mNavigationHandler.setParentView(this);
            mTabObserver = (tab, hint) -> mNavigationHandler.reset();
        } else {
            mDetector = null;
            mTabProvider = null;
            mNavigationHandler = null;
            mTabObserver = null;
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (mTabObserver != null) mTabProvider.addObserverAndTrigger(mTabObserver);
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (mTabObserver != null) mTabProvider.removeObserver(mTabObserver);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent e) {
        if (mDetector != null) {
            mDetector.onTouchEvent(e);
            mNavigationHandler.onTouchEvent(e);
        }
        return super.dispatchTouchEvent(e);
    }

    private class SideNavGestureListener extends GestureDetector.SimpleOnGestureListener {
        @Override
        public boolean onDown(MotionEvent event) {
            return mNavigationHandler.onDown(event);
        }

        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            // onScroll needs handling only after the state moves away from none state.
            if (mNavigationHandler.isStopped()) return true;

            if (wasLastSideSwipeGestureConsumed()) {
                mNavigationHandler.reset();
                return true;
            }
            return mNavigationHandler.onScroll(distanceX, distanceY);
        }
    }

    /**
     * Checks if the gesture event was consumed by one of children views, in which case
     * history navigation should not proceed. Whatever the child view does with the gesture
     * events should take precedence and not be disturbed by the navigation.
     *
     * @return {@code true} if gesture event is consumed by one of the children.
     */
    public boolean wasLastSideSwipeGestureConsumed() {
        return false;
    }

    /**
     * Cancel navigation UI with animation effect.
     */
    public void release() {
        if (mNavigationHandler != null) mNavigationHandler.release(false);
    }
}
