// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.animation.TimeAnimator;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.SystemClock;
import android.util.Log;
import android.view.Display;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.view.View.OnTouchListener;
import android.view.ViewConfiguration;
import android.view.ViewParent;
import android.widget.ListPopupWindow;
import android.widget.ListView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.UmaBridge;

import java.util.ArrayList;

/**
 * Handles the drag touch events on AppMenu that start from the menu button.
 *
 * Lint suppression for NewApi is added because we are using TimeAnimator class that was marked
 * hidden in API 16.
 */
@SuppressLint("NewApi")
class AppMenuDragHelper {
    private static final String TAG = "AppMenuDragHelper";

    private final Activity mActivity;
    private final AppMenu mAppMenu;

    // Internally used action constants for dragging.
    private static final int ITEM_ACTION_HIGHLIGHT = 0;
    private static final int ITEM_ACTION_PERFORM = 1;
    private static final int ITEM_ACTION_CLEAR_HIGHLIGHT_ALL = 2;

    private static final float AUTO_SCROLL_AREA_MAX_RATIO = 0.25f;
    private static final int EDGE_SWIPE_IN_ADDITIONAL_SLOP_TIME_MS = 500;

    // Dragging related variables, i.e., menu showing initiated by touch down and drag to navigate.
    private final float mAutoScrollFullVelocity;
    private final int mEdgeSwipeInSlop;
    private final int mEdgeSwipeInAdditionalSlop;
    private final int mEdgeSwipeOutSlop;
    private int mScaledTouchSlop;
    private long mHardwareMenuButtonUpTime;
    private boolean mDragPending;
    private final TimeAnimator mDragScrolling = new TimeAnimator();
    private float mDragScrollOffset;
    private int mDragScrollOffsetRounded;
    private volatile float mDragScrollingVelocity;
    private volatile float mLastTouchX;
    private volatile float mLastTouchY;
    private float mTopTouchMovedBound;
    private float mBottomTouchMovedBound;
    private boolean mIsDownScrollable;
    private boolean mIsUpScrollable;
    private boolean mIsByHardwareButton;
    private int mCurrentScreenRotation = -1;

    // These are used in a function locally, but defined here to avoid heap allocation on every
    // touch event.
    private final Rect mScreenVisibleRect = new Rect();
    private final int[] mScreenVisiblePoint = new int[2];

    // Sub-UI-controls, backward, forward, bookmark and listView, are getting a touch event first
    // if the app menu is initiated by hardware menu button. For those cases, we need to
    // conditionally forward the touch event to our drag scrolling method.
    private final OnTouchListener mDragScrollTouchEventForwarder = new OnTouchListener() {
        @Override
        public boolean onTouch(View view, MotionEvent event) {
            return handleDragging(event);
        }
    };

    AppMenuDragHelper(Activity activity, AppMenu appMenu) {
        mActivity = activity;
        mAppMenu = appMenu;
        mScaledTouchSlop = ViewConfiguration.get(
                mActivity.getApplicationContext()).getScaledTouchSlop();
        Resources res = mActivity.getResources();
        mAutoScrollFullVelocity = res.getDimensionPixelSize(R.dimen.auto_scroll_full_velocity);
        mEdgeSwipeInSlop = res.getDimensionPixelSize(R.dimen.edge_swipe_in_slop);
        mEdgeSwipeInAdditionalSlop = res.getDimensionPixelSize(
                R.dimen.edge_swipe_in_additional_slop);
        mEdgeSwipeOutSlop = res.getDimensionPixelSize(R.dimen.edge_swipe_out_slop);
        // If user is dragging and the popup ListView is too big to display at once,
        // mDragScrolling animator scrolls mPopup.getListView() automatically depending on
        // the user's touch position.
        mDragScrolling.setTimeListener(new TimeAnimator.TimeListener() {
            @Override
            public void onTimeUpdate(TimeAnimator animation, long totalTime, long deltaTime) {
                ListPopupWindow popup = mAppMenu.getPopup();
                if (popup == null || popup.getListView() == null) return;

                // We keep both mDragScrollOffset and mDragScrollOffsetRounded because
                // the actual scrolling is by the rounded value but at the same time we also
                // want to keep the precise scroll value in float.
                mDragScrollOffset += (deltaTime * 0.001f) * mDragScrollingVelocity;
                int diff = Math.round(mDragScrollOffset - mDragScrollOffsetRounded);
                mDragScrollOffsetRounded += diff;
                popup.getListView().smoothScrollBy(diff, 0);

                // Force touch move event to highlight items correctly for the scrolled position.
                if (!Float.isNaN(mLastTouchX) && !Float.isNaN(mLastTouchY)) {
                    int actionToPerform = isInSwipeOutRegion(mLastTouchX, mLastTouchY) ?
                            ITEM_ACTION_CLEAR_HIGHLIGHT_ALL : ITEM_ACTION_HIGHLIGHT;
                    menuItemAction(Math.round(mLastTouchX), Math.round(mLastTouchY),
                            actionToPerform);
                }
            }
        });
    }

    /**
     * Sets up all the internal state to prepare for menu dragging.
     *
     * @param isByHardwareButton Whether or not hardware button triggered it. (oppose to software
     *                           button)
     * @param startDragging      Whether dragging is started. For example, if the app menu
     *                           is showed by tapping on a button, this should be false. If it is
     *                           showed by start dragging down on the menu button, this should be
     *                           true. Note that if isByHardwareButton is true, this is ignored.
     */
    void onShow(boolean isByHardwareButton, boolean startDragging) {
        mCurrentScreenRotation = mActivity.getWindowManager().getDefaultDisplay().getRotation();
        mLastTouchX = Float.NaN;
        mLastTouchY = Float.NaN;
        mDragScrollOffset = 0.0f;
        mDragScrollOffsetRounded = 0;
        mDragScrollingVelocity = 0.0f;

        mIsByHardwareButton = isByHardwareButton;
        mDragPending = isByHardwareButton;
        mIsDownScrollable = !isByHardwareButton;
        mIsUpScrollable = !isByHardwareButton;

        mTopTouchMovedBound = Float.POSITIVE_INFINITY;
        mBottomTouchMovedBound = Float.NEGATIVE_INFINITY;
        mHardwareMenuButtonUpTime = -1;

        ListPopupWindow popup = mAppMenu.getPopup();
        popup.getListView().setOnTouchListener(mDragScrollTouchEventForwarder);

        // We assume that the parent of popup ListView is an instance of View. Otherwise, dragging
        // from a hardware menu button won't work.
        ViewParent listViewParent = popup.getListView().getParent();
        if (listViewParent instanceof View) {
            ((View) listViewParent).setOnTouchListener(mDragScrollTouchEventForwarder);
        } else {
            assert false;
        }

        if (mAppMenu.isShowingIconRow()) {
            View iconRowView = mAppMenu.getIconRowView();
            iconRowView.findViewById(R.id.menu_item_back).setOnTouchListener(
                    mDragScrollTouchEventForwarder);
            iconRowView.findViewById(R.id.menu_item_forward).setOnTouchListener(
                    mDragScrollTouchEventForwarder);
            iconRowView.findViewById(R.id.menu_item_bookmark).setOnTouchListener(
                    mDragScrollTouchEventForwarder);

        }

        if (!isByHardwareButton && startDragging) mDragScrolling.start();
    }

    void onDismiss() {
        mDragScrolling.cancel();
    }

    /**
     * This is a hint for adjusting edgeSwipeInSlop. For example. If the touch event started
     * immediately after hardware menu button up, then we use larger edgeSwipeInSlop because it
     * implies user is swiping in fast.
     */
    public void hardwareMenuButtonUp() {
        // There should be only one time hardware menu button up.
        assert mHardwareMenuButtonUpTime == -1;
        mHardwareMenuButtonUpTime = SystemClock.uptimeMillis();
    }

    /**
     * Gets all the touch events and updates dragging related logic. Note that if this app menu
     * is initiated by software UI control, then the control should set onTouchListener and forward
     * all the events to this method because the initial UI control that processed ACTION_DOWN will
     * continue to get all the subsequent events.
     *
     * @param event Touch event to be processed.
     * @return Whether the event is handled.
     */
    boolean handleDragging(MotionEvent event) {
        if (!mAppMenu.isShowing() || (!mDragPending && !mDragScrolling.isRunning())) return false;

        // We will only use the screen space coordinate (rawX, rawY) to reduce confusion.
        // This code works across many different controls, so using local coordinates will be
        // a disaster.

        final float rawX = event.getRawX();
        final float rawY = event.getRawY();
        final int roundedRawX = Math.round(rawX);
        final int roundedRawY = Math.round(rawY);
        final int eventActionMasked = event.getActionMasked();
        final ListView listView = mAppMenu.getPopup().getListView();

        mLastTouchX = rawX;
        mLastTouchY = rawY;

        // Because (hardware) menu button can be right or left side of the screen, if we just
        // trigger auto scrolling based on Y inside the listView, it might be scrolled
        // unintentionally. Therefore, we will require touch position to move up or down a certain
        // amount of distance to trigger auto scrolling up or down.
        mTopTouchMovedBound = Math.min(mTopTouchMovedBound, rawY);
        mBottomTouchMovedBound = Math.max(mBottomTouchMovedBound, rawY);
        if (rawY <= mBottomTouchMovedBound - mScaledTouchSlop) {
            mIsUpScrollable = true;
        }
        if (rawY >= mTopTouchMovedBound + mScaledTouchSlop) {
            mIsDownScrollable = true;
        }

        if (eventActionMasked == MotionEvent.ACTION_CANCEL) {
            mAppMenu.dismiss();
            return true;
        }

        if (eventActionMasked == MotionEvent.ACTION_DOWN) {
            assert mIsByHardwareButton != mDragScrolling.isStarted();
            if (mIsByHardwareButton) {
                if (mDragPending && getDistanceFromHardwareMenuButtonSideEdge(rawX, rawY) <
                        getEdgeSwipeInSlop(event)) {
                    mDragScrolling.start();
                    mDragPending = false;
                    UmaBridge.usingMenu(true, true);
                } else {
                    if (!getScreenVisibleRect(listView).contains(roundedRawX, roundedRawY)) {
                        mAppMenu.dismiss();
                    }
                    mDragPending = false;
                    UmaBridge.usingMenu(true, false);
                    return false;
                }
            }
        }

        // After this line, drag scrolling is happening.
        if (!mDragScrolling.isRunning()) return false;

        boolean didPerformClick = false;
        int itemAction = ITEM_ACTION_CLEAR_HIGHLIGHT_ALL;
        if (!isInSwipeOutRegion(rawX, rawY)) {
            switch (eventActionMasked) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_MOVE:
                    itemAction = ITEM_ACTION_HIGHLIGHT;
                    break;
                case MotionEvent.ACTION_UP:
                    itemAction = ITEM_ACTION_PERFORM;
                    break;
                default:
                    break;
            }
        }
        didPerformClick = menuItemAction(roundedRawX, roundedRawY, itemAction);

        if (eventActionMasked == MotionEvent.ACTION_UP && !didPerformClick) {
            mAppMenu.dismiss();
        } else if (eventActionMasked == MotionEvent.ACTION_MOVE) {
            // Auto scrolling on the top or the bottom of the listView.
            if (listView.getHeight() > 0) {
                float autoScrollAreaRatio = Math.min(AUTO_SCROLL_AREA_MAX_RATIO,
                        mAppMenu.getItemRowHeight() * 1.2f / listView.getHeight());
                float normalizedY =
                        (rawY - getScreenVisibleRect(listView).top) / listView.getHeight();
                if (mIsUpScrollable && normalizedY < autoScrollAreaRatio) {
                    // Top
                    mDragScrollingVelocity = (normalizedY / autoScrollAreaRatio - 1.0f)
                            * mAutoScrollFullVelocity;
                } else if (mIsDownScrollable && normalizedY > 1.0f - autoScrollAreaRatio) {
                    // Bottom
                    mDragScrollingVelocity = ((normalizedY - 1.0f) / autoScrollAreaRatio + 1.0f)
                            * mAutoScrollFullVelocity;
                } else {
                    // Middle or not scrollable.
                    mDragScrollingVelocity = 0.0f;
                }
            }
        }

        return true;
    }

    /**
     * @return Whether or not the position should be considered swiping-out, if ACTION_UP happens
     *         at the position.
     */
    private boolean isInSwipeOutRegion(float rawX, float rawY) {
        return getShortestDistanceFromEdge(rawX, rawY) < mEdgeSwipeOutSlop;
    }

    /**
     * @return The shortest distance from the screen edges for the given position rawX, rawY
     *         in screen coordinates.
     */
    private float getShortestDistanceFromEdge(float rawX, float rawY) {
        Display display = mActivity.getWindowManager().getDefaultDisplay();
        Point displaySize = new Point();
        display.getSize(displaySize);

        float distance = Math.min(
                Math.min(rawY, displaySize.y - rawY - 1),
                Math.min(rawX, displaySize.x - rawX - 1));
        if (distance < 0.0f) {
            Log.d(TAG, "Received touch event out of the screen edge boundary. distance = " +
                    distance);
        }
        return Math.abs(distance);
    }

    /**
     * Performs the specified action on the menu item specified by the screen coordinate position.
     * @param screenX X in screen space coordinate.
     * @param screenY Y in screen space coordinate.
     * @param action  Action type to perform, it should be one of ITEM_ACTION_* constants.
     * @return true whether or not a menu item is performed (executed).
     */
    private boolean menuItemAction(int screenX, int screenY, int action) {
        ListView listView = mAppMenu.getPopup().getListView();

        ArrayList<View> itemViews = new ArrayList<View>();
        for (int i = 0; i < listView.getChildCount(); ++i) {
            itemViews.add(listView.getChildAt(i));
        }

        View iconRowView = mAppMenu.getIconRowView();
        if (iconRowView != null && mAppMenu.isShowingIconRow()) {
            itemViews.add(iconRowView.findViewById(R.id.menu_item_back));
            itemViews.add(iconRowView.findViewById(R.id.menu_item_forward));
            itemViews.add(iconRowView.findViewById(R.id.menu_item_bookmark));
        }

        boolean didPerformClick = false;
        for (int i = 0; i < itemViews.size(); ++i) {
            View itemView = itemViews.get(i);

            // Skip the icon row that belongs to the listView because that doesn't really
            // exist as an item.
            int listViewPositionIndex = listView.getFirstVisiblePosition() + i;
            if (mAppMenu.isShowingIconRow() && listViewPositionIndex == 0) continue;

            boolean shouldPerform = itemView.isEnabled() && itemView.isShown() &&
                    getScreenVisibleRect(itemView).contains(screenX, screenY);

            switch (action) {
                case ITEM_ACTION_HIGHLIGHT:
                    itemView.setPressed(shouldPerform);
                    break;
                case ITEM_ACTION_PERFORM:
                    if (shouldPerform) {
                        if (itemView.getParent() == listView) {
                            listView.performItemClick(itemView, listViewPositionIndex, 0);
                        } else {
                            itemView.performClick();
                        }
                        didPerformClick = true;
                    }
                    break;
                case ITEM_ACTION_CLEAR_HIGHLIGHT_ALL:
                    itemView.setPressed(false);
                    break;
                default:
                    assert false;
                    break;
            }
        }
        return didPerformClick;
    }

    /**
     * @return The distance from the screen edge that is likely where the hardware menu button is
     *         located at. We assume the hardware menu button is at the bottom in the default,
     *         ROTATION_0, rotation. Note that there is a bug filed for Android API to request
     *         hardware menu button position b/10007237.
     */
    private float getDistanceFromHardwareMenuButtonSideEdge(float rawX, float rawY) {
        Display display = mActivity.getWindowManager().getDefaultDisplay();
        Point displaySize = new Point();
        display.getSize(displaySize);

        float distance;
        switch (mCurrentScreenRotation) {
            case Surface.ROTATION_0:
                distance = displaySize.y - rawY - 1;
                break;
            case Surface.ROTATION_180:
                distance = rawY;
                break;
            case Surface.ROTATION_90:
                distance = displaySize.x - rawX - 1;
                break;
            case Surface.ROTATION_270:
                distance = rawX;
                break;
            default:
                distance = 0.0f;
                assert false;
                break;
        }
        if (distance < 0.0f) {
            Log.d(TAG, "Received touch event out of hardware menu button side edge boundary." +
                    " distance = " + distance);
        }
        return Math.abs(distance);
    }

    /**
     * @return Visible rect in screen coordinates for the given View.
     */
    private Rect getScreenVisibleRect(View view) {
        view.getLocalVisibleRect(mScreenVisibleRect);
        view.getLocationOnScreen(mScreenVisiblePoint);
        mScreenVisibleRect.offset(mScreenVisiblePoint[0], mScreenVisiblePoint[1]);
        return mScreenVisibleRect;
    }

    /**
     * Computes Edge-swipe-in-slop and returns it.
     *
     * When user swipes in from a hardware menu button, because the swiping-in touch event doesn't
     * necessarily start form the exact edge, we should also consider slightly more inside touch
     * event as swiping-in. This value, Edge-swipe-in-slop, is the threshold distance from the
     * edge that separates swiping-in and normal touch.
     *
     * @param event Touch event that eventually made this call.
     * @return Edge-swipe-in-slop.
     */
    private float getEdgeSwipeInSlop(MotionEvent event) {
        float edgeSwipeInSlope = mEdgeSwipeInSlop;
        if (mHardwareMenuButtonUpTime == -1) {
            // Hardware menu hasn't even had UP event yet. That means, user is swiping in really
            // really fast. So use large edgeSwipeInSlope.
            edgeSwipeInSlope += mEdgeSwipeInAdditionalSlop;
        } else {
            // If it's right after we had hardware menu button UP event, use large edgeSwipeInSlop,
            // Otherwise, use small edgeSwipeInSlop.
            float additionalEdgeSwipeInSlop = ((mHardwareMenuButtonUpTime - event.getEventTime()
                    + EDGE_SWIPE_IN_ADDITIONAL_SLOP_TIME_MS) * 0.001f)
                    * mEdgeSwipeInAdditionalSlop;
            edgeSwipeInSlope += Math.max(0.0f, additionalEdgeSwipeInSlop);
        }
        return edgeSwipeInSlope;
    }
}
