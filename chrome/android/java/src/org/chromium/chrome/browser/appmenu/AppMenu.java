// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.animation.TimeAnimator;
import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Display;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnKeyListener;
import android.view.View.OnTouchListener;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.HeaderViewListAdapter;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ListPopupWindow;
import android.widget.ListView;
import android.widget.ListView.FixedViewInfo;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;
import android.widget.TextView;

import com.google.common.annotations.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.BookmarksBridge;
import org.chromium.chrome.browser.UmaBridge;
import org.chromium.chrome.browser.util.KeyNavigationUtil;

import java.util.ArrayList;
import java.util.List;

/**
 * Shows a popup of menuitems anchored to a host view. When a item is selected we call
 * Activity.onOptionsItemSelected with the appropriate MenuItem.
 *   - Only visible MenuItems are shown.
 *   - Disabled items are grayed out.
 */
public class AppMenu implements AdapterView.OnItemClickListener, OnKeyListener {
    private static final String TAG = "AppMenu";

    private static final float LAST_ITEM_SHOW_FRACTION = 0.5f;
    private static final int DIVIDER_HEIGHT_DP = 1;

    private static final float AUTO_SCROLL_AREA_MAX_RATIO = 0.25f;
    private static final int EDGE_SWIPE_IN_ADDITIONAL_SLOP_TIME_MS = 500;

    // Internally used action constants for dragging.
    private static final int ITEM_ACTION_HIGHLIGHT = 0;
    private static final int ITEM_ACTION_PERFORM = 1;
    private static final int ITEM_ACTION_CLEAR_HIGHLIGHT_ALL = 2;

    private final Menu mMenu;
    private final Activity mActivity;
    private final int mItemRowHeight;
    private final int mItemDividerHeight;
    private final int mVerticalFadeDistance;
    private final int mAdditionalVerticalOffset;
    private ListPopupWindow mPopup;
    private LayoutInflater mInflater;
    private boolean mShowIconRow;
    private MenuAdapter mAdapter;
    private ImageButton mBookmarkButton;
    private View mIconRowView;
    private AppMenuHandler mHandler;

    // Dragging related variables, i.e., menu showing initiated by touch down and drag to navigate.
    private final float mAutoScrollFullVelocity;
    private final int mEdgeSwipeInSlop;
    private final int mEdgeSwipeInAdditionalSlop;
    private final int mEdgeSwipeOutSlop;
    private int mScaledTouchSlop;
    private long mHardwareMenuButtonUpTime;
    private boolean mIsByHardwareButton;
    private boolean mDragPending;
    private final TimeAnimator mDragScrolling = new TimeAnimator();
    private float mDragScrollOffset;
    private int mDragScrollOffsetRounded;
    private volatile float mDragScrollingVelocity;
    private volatile float mLastTouchX;
    private volatile float mLastTouchY;
    private int mCurrentScreenRotation = -1;
    private float mTopTouchMovedBound;
    private float mBottomTouchMovedBound;
    private boolean mIsDownScrollable;
    private boolean mIsUpScrollable;

    // Sub-UI-controls, backward, forward, bookmark and listView, are getting a touch event first
    // if the app menu is initiated by hardware menu button. For those cases, we need to
    // conditionally forward the touch event to our drag scrolling method.
    private final OnTouchListener mDragScrollTouchEventForwarder = new OnTouchListener() {
        @Override
        public boolean onTouch(View view, MotionEvent event) {
            return AppMenu.this.handleDragging(event);
        }
    };

    // These are used in a function locally, but defined here to avoid heap allocation on every
    // touch event.
    private final Rect mScreenVisibleRect = new Rect();
    private final int[] mScreenVisiblePoint = new int[2];

    /**
     * Creates and sets up the App Menu.
     * @param activity Activity that will handle app menu callbacks.
     * @param menu Original menu created by the framework.
     * @param itemRowHeight Desired height for each app menu row.
     */
    AppMenu(Activity activity, Menu menu, int itemRowHeight, AppMenuHandler handler) {
        mActivity = activity;
        mMenu = menu;
        mScaledTouchSlop =
                ViewConfiguration.get(mActivity.getApplicationContext()).getScaledTouchSlop();
        mItemRowHeight = itemRowHeight;
        assert mItemRowHeight > 0;

        mHandler = handler;
        Resources res = mActivity.getResources();

        final float dpToPx = res.getDisplayMetrics().density;
        mItemDividerHeight = (int) (DIVIDER_HEIGHT_DP * dpToPx);

        mAdditionalVerticalOffset = res.getDimensionPixelSize(R.dimen.menu_vertical_offset);
        mVerticalFadeDistance = res.getDimensionPixelSize(R.dimen.menu_vertical_fade_distance);
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
                if (mPopup == null || mPopup.getListView() == null) return;

                // We keep both mDragScrollOffset and mDragScrollOffsetRounded because
                // the actual scrolling is by the rounded value but at the same time we also
                // want to keep the precise scroll value in float.
                mDragScrollOffset += (deltaTime * 0.001f) * mDragScrollingVelocity;
                int diff = Math.round(mDragScrollOffset - mDragScrollOffsetRounded);
                mDragScrollOffsetRounded += diff;
                mPopup.getListView().smoothScrollBy(diff, 0);

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

    private void updateBookmarkButton() {
        final MenuItem bookmarkMenuItem = mMenu.findItem(R.id.bookmark_this_page_id);
        if (mBookmarkButton == null || bookmarkMenuItem == null) return;
        if (bookmarkMenuItem.isEnabled()) {
            mBookmarkButton.setImageResource(R.drawable.star);
            mBookmarkButton.setContentDescription(mBookmarkButton.getContext().getString(
                    R.string.accessibility_menu_bookmark));
        } else {
            mBookmarkButton.setImageResource(R.drawable.star_lit);
            mBookmarkButton.setContentDescription(mBookmarkButton.getContext().getString(
                    R.string.accessibility_menu_edit_bookmark));
        }
    }

    /**
     * Creates and shows the app menu anchored to the specified view.
     *
     * @param context            The context of the app popup (ensure the proper theme is set on
     *                           this context).
     * @param anchorView         The anchor {@link View} of the {@link ListPopupWindow}.
     * @param showIconRow        Whether or not the icon row should be shown,
     * @param isByHardwareButton Whether or not hardware button triggered it. (oppose to software
     *                           button)
     * @param startDragging      Whether dragging is started. For example, if the app menu
     *                           is showed by tapping on a button, this should be false. If it is
     *                           showed by start dragging down on the menu button, this should be
     *                           true. Note that if isByHardwareButton is true, this is ignored.
     */
    void show(Context context, View anchorView, boolean showIconRow,
            boolean isByHardwareButton, boolean startDragging) {
        mPopup = new ListPopupWindow(context, null, android.R.attr.popupMenuStyle);
        mInflater = LayoutInflater.from(context);
        mPopup.setModal(true);
        mPopup.setAnchorView(anchorView);
        mPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
        mPopup.setOnDismissListener(new OnDismissListener() {
            @Override
            public void onDismiss() {
                if (mPopup.getAnchorView() instanceof ImageButton) {
                    ((ImageButton) mPopup.getAnchorView()).setSelected(false);
                }
                mHandler.onMenuVisibilityChanged(false, ListView.INVALID_POSITION);
            }
        });
        mPopup.setWidth(context.getResources().getDimensionPixelSize(R.dimen.menu_width));

        mShowIconRow = showIconRow;
        mCurrentScreenRotation = mActivity.getWindowManager().getDefaultDisplay().getRotation();
        mIsByHardwareButton = isByHardwareButton;

        // Extract visible items from the Menu.
        int numItems = mMenu.size();
        List<MenuItem> menuItems = new ArrayList<MenuItem>();
        for (int i = 0; i < numItems; ++i) {
            MenuItem item = mMenu.getItem(i);
            if (item.isVisible()) {
                menuItems.add(item);
            }
        }

        // A List adapter for visible items in the Menu. The first row is added as a header to the
        // list view.
        mAdapter = new MenuAdapter(menuItems, mInflater);
        if (mShowIconRow) {
            mIconRowView = mInflater.inflate(R.layout.menu_icon_row, null);
            // Add click handlers for the header icons.
            setIconRowEventHandlers(mIconRowView, mActivity);
            updateBookmarkButton();

            // Icon row goes into header of List view.
            ArrayList<FixedViewInfo> headerInfoList =
                    populateHeaderViewInfo(mActivity.getApplicationContext(), mIconRowView);

            HeaderViewListAdapter headerViewListAdapter = new HeaderViewListAdapter(
                    headerInfoList, null, mAdapter);
            mPopup.setAdapter(headerViewListAdapter);
        } else {
            mPopup.setAdapter(mAdapter);
        }

        // Get the height and width of the display.
        Rect appRect = new Rect();
        mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(appRect);

        setMenuHeight(menuItems.size() + (mShowIconRow ? 1 : 0), appRect);
        setPopupOffset(mPopup, mCurrentScreenRotation, appRect);
        mPopup.setOnItemClickListener(this);
        mPopup.show();
        mPopup.getListView().setDividerHeight(mItemDividerHeight);

        mHandler.onMenuVisibilityChanged(true, getCurrentFocusedPosition());

        if (mVerticalFadeDistance > 0) {
            mPopup.getListView().setVerticalFadingEdgeEnabled(true);
            mPopup.getListView().setFadingEdgeLength(mVerticalFadeDistance);
        }

        mPopup.getListView().setOnKeyListener(this);

        // Initiate drag related variables and listeners.
        mLastTouchX = Float.NaN;
        mLastTouchY = Float.NaN;
        mDragScrollOffset = 0.0f;
        mDragScrollOffsetRounded = 0;
        mDragScrollingVelocity = 0.0f;

        mDragPending = isByHardwareButton;
        mIsDownScrollable = !isByHardwareButton;
        mIsUpScrollable = !isByHardwareButton;

        mTopTouchMovedBound = Float.POSITIVE_INFINITY;
        mBottomTouchMovedBound = Float.NEGATIVE_INFINITY;
        mHardwareMenuButtonUpTime = -1;

        // Handles dragging related logic.
        mPopup.getListView().setOnTouchListener(mDragScrollTouchEventForwarder);

        // We assume that the parent of popup ListView is an instance of View. Otherwise, dragging
        // from a hardware menu button won't work.
        ViewParent listViewParent = mPopup.getListView().getParent();
        if (listViewParent instanceof View) {
            ((View) listViewParent).setOnTouchListener(mDragScrollTouchEventForwarder);
        } else {
            assert false;
        }

        if (!isByHardwareButton && startDragging) mDragScrolling.start();
    }

    private void setPopupOffset(ListPopupWindow popup, int screenRotation, Rect appRect) {
        Rect paddingRect = new Rect();
        popup.getBackground().getPadding(paddingRect);
        int[] anchorLocation = new int[2];
        popup.getAnchorView().getLocationInWindow(anchorLocation);

        // If we have a hardware menu button, locate the app menu closer to the estimated
        // hardware menu button location.
        if (mIsByHardwareButton) {
            int horizontalOffset = -anchorLocation[0];
            switch (screenRotation) {
                case Surface.ROTATION_0:
                case Surface.ROTATION_180:
                    horizontalOffset += (appRect.width() - mPopup.getWidth()) / 2;
                    break;
                case Surface.ROTATION_90:
                    horizontalOffset += appRect.width() - mPopup.getWidth();
                    break;
                case Surface.ROTATION_270:
                    break;
                default:
                    assert false;
                    break;
            }
            popup.setHorizontalOffset(horizontalOffset);
            // The menu is displayed above the anchored view, so shift the menu up by the top
            // padding of the background.
            popup.setVerticalOffset(mAdditionalVerticalOffset - paddingRect.bottom);
        } else {
            // The menu is displayed below the anchored view, so shift the menu up by the top
            // padding of the background.
            popup.setVerticalOffset(mAdditionalVerticalOffset - paddingRect.top);
        }
    }

    @Override
    public boolean onKey(View v, int keyCode, KeyEvent event) {
        if (mPopup == null || mPopup.getListView() == null) return false;

        ListView listView = mPopup.getListView();
        if (KeyNavigationUtil.isGoUp(event)) {
            int previousPosition = listView.getSelectedItemPosition();
            boolean handled = mPopup.onKeyDown(keyCode, event);
            if (listView.getSelectedItemPosition() == ListView.INVALID_POSITION) {
                listView.setSelection(0);
            }

            if (mShowIconRow && previousPosition == 1) {
                // Clearing the selection is required to move into the icon row.
                mPopup.clearListSelection();
                requestFocusToEnabledIconRowButton();
            }

            mHandler.onKeyboardFocusChanged(getCurrentFocusedPosition());
            return handled;
        } else if (KeyNavigationUtil.isGoDown(event)) {
            boolean handled = mPopup.onKeyDown(keyCode, event);
            if (listView.getSelectedItemPosition() == ListView.INVALID_POSITION) {
                listView.setSelection(listView.getCount() - 1);
            }
            mHandler.onKeyboardFocusChanged(getCurrentFocusedPosition());
            return handled;
        } else if (KeyNavigationUtil.isEnter(event)) {
            int position = getCurrentFocusedPosition();
            if (mShowIconRow && position == 0) return false;

            int adjustedPosition = mShowIconRow ? position - 1 : position;
            MenuItem clickedItem = mAdapter.getItem(adjustedPosition);
            dismiss();
            mActivity.onOptionsItemSelected(clickedItem);
            mHandler.onKeyboardActivatedItem(position);
            return true;
        } else if (event.getKeyCode() == KeyEvent.KEYCODE_MENU) {
            if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) {
                event.startTracking();
                v.getKeyDispatcherState().startTracking(event, this);
                return true;
            } else if (event.getAction() == KeyEvent.ACTION_UP) {
                v.getKeyDispatcherState().handleUpEvent(event);
                if (event.isTracking() && !event.isCanceled()) {
                    dismiss();
                    return true;
                }
            }
            return false;
        }

        return false;
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
     * @return Whether or not the position should be considered swiping-out, if ACTION_UP happens
     *         at the position.
     */
    private boolean isInSwipeOutRegion(float rawX, float rawY) {
        return getShortestDistanceFromEdge(rawX, rawY) < mEdgeSwipeOutSlop;
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
        if (!isShowing() || (!mDragPending && !mDragScrolling.isRunning())) return false;

        // We will only use the screen space coordinate (rawX, rawY) to reduce confusion.
        // This code works across many different controls, so using local coordinates will be
        // a disaster.

        final float rawX = event.getRawX();
        final float rawY = event.getRawY();
        final int roundedRawX = Math.round(rawX);
        final int roundedRawY = Math.round(rawY);
        final int eventActionMasked = event.getActionMasked();
        final ListView listView = mPopup.getListView();

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
            dismiss();
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
                        dismiss();
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
            dismiss();
        } else if (eventActionMasked == MotionEvent.ACTION_MOVE) {
            // Auto scrolling on the top or the bottom of the listView.
            if (listView.getHeight() > 0) {
                float autoScrollAreaRatio = Math.min(AUTO_SCROLL_AREA_MAX_RATIO,
                        mItemRowHeight * 1.2f / listView.getHeight());
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
     * Performs the specified action on the menu item specified by the screen coordinate position.
     * @param screenX X in screen space coordinate.
     * @param screenY Y in screen space coordinate.
     * @param action  Action type to perform, it should be one of ITEM_ACTION_* constants.
     * @return true whether or not a menu item is performed (executed).
     */
    private boolean menuItemAction(int screenX, int screenY, int action) {
        ListView listView = mPopup.getListView();

        ArrayList<View> itemViews = new ArrayList<View>();
        for (int i = 0; i < listView.getChildCount(); ++i) {
            itemViews.add(listView.getChildAt(i));
        }

        if (mIconRowView != null && mShowIconRow) {
            itemViews.add(mIconRowView.findViewById(R.id.menu_item_back));
            itemViews.add(mIconRowView.findViewById(R.id.menu_item_forward));
            itemViews.add(mIconRowView.findViewById(R.id.menu_item_bookmark));
        }

        boolean didPerformClick = false;
        for (int i = 0; i < itemViews.size(); ++i) {
            View itemView = itemViews.get(i);

            // Skip the icon row that belongs to the listView because that doesn't really
            // exist as an item.
            int listViewPositionIndex = listView.getFirstVisiblePosition() + i;
            if (mShowIconRow && listViewPositionIndex == 0) continue;

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
     * Requests focus whichever button in icon row is enabled from the left.
     */
    private void requestFocusToEnabledIconRowButton() {
        View backView = mIconRowView.findViewById(R.id.menu_item_back);
        View forwardView = mIconRowView.findViewById(R.id.menu_item_forward);
        View bookmarkView = mIconRowView.findViewById(R.id.menu_item_bookmark);
        if (backView.isFocusable()) {
            backView.requestFocus();
        } else if (forwardView.isFocusable()) {
            forwardView.requestFocus();
        } else {
            bookmarkView.requestFocus();
        }
    }

    private void setMenuHeight(int numMenuItems, Rect appDimensions) {
        assert mPopup.getAnchorView() != null;
        View anchorView = mPopup.getAnchorView();
        int[] anchorViewLocation = new int[2];
        anchorView.getLocationOnScreen(anchorViewLocation);
        anchorViewLocation[1] -= appDimensions.top;

        int availableScreenSpace = Math.max(anchorViewLocation[1],
                appDimensions.height() - anchorViewLocation[1] - anchorView.getHeight());

        Rect padding = new Rect();
        mPopup.getBackground().getPadding(padding);

        if (mIsByHardwareButton) {
            availableScreenSpace -= padding.top;
        } else {
            availableScreenSpace -= padding.bottom;
        }

        int numCanFit = availableScreenSpace / (mItemRowHeight + mItemDividerHeight);

        // Fade out the last item if we cannot fit all items.
        if (numCanFit < numMenuItems) {
            int spaceForFullItems = numCanFit * (mItemRowHeight + mItemDividerHeight);
            int spaceForPartialItem = (int) (LAST_ITEM_SHOW_FRACTION * mItemRowHeight);
            // Determine which item needs hiding.
            if (spaceForFullItems + spaceForPartialItem < availableScreenSpace) {
                mPopup.setHeight(spaceForFullItems + spaceForPartialItem +
                        padding.top + padding.bottom);
            } else {
                mPopup.setHeight(spaceForFullItems - mItemRowHeight + spaceForPartialItem +
                        padding.top + padding.bottom);
            }
        } else {
            mPopup.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        }
    }

    private ArrayList<FixedViewInfo> populateHeaderViewInfo(Context context, View headerView) {
        ArrayList<FixedViewInfo> headerInfoList = new ArrayList<FixedViewInfo>();
        ListView lv = new ListView(context);
        FixedViewInfo viewInfo = lv.new FixedViewInfo();
        viewInfo.view = headerView;
        // Make header not selectable, we handle the clicks on our own.
        viewInfo.isSelectable = false;
        headerInfoList.add(viewInfo);
        return headerInfoList;
    }

    /**
     * Adds click handlers for items in the icon row.
     * Also disable/enable the view based on the menu item.
     * We assume that we have Back, Forward and Bookmark-star icons in this view.
     */
    private void setIconRowEventHandlers(View iconRowView, final Activity activity) {
        final MenuItem backMenuItem = mMenu.findItem(R.id.back_menu_id);
        final MenuItem forwardMenuItem = mMenu.findItem(R.id.forward_menu_id);
        final MenuItem bookmarkMenuItem = mMenu.findItem(R.id.bookmark_this_page_id);

        View.OnFocusChangeListener focusListener = new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                mHandler.onKeyboardFocusChanged(getCurrentFocusedPosition());
            }
        };

        OnKeyListener keyListener = new OnKeyListener() {
            @Override
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                if (isShowing() && v.isFocused()) {
                    if (keyCode == KeyEvent.KEYCODE_MENU) {
                        v.setSelected(false);
                        dismiss();
                        return true;
                    } else if (KeyNavigationUtil.isGoUp(event)) {
                        // Catch attempts to move out of bounds.
                        mHandler.onKeyboardFocusChanged(getCurrentFocusedPosition());
                        return true;
                    } else if (KeyNavigationUtil.isGoDown(event)) {
                        // Requesting focus on the mPopup.getListView().getChildAt(0) does not work.
                        // Requesting the focus on the ListView focuses the first non-header item.
                        mPopup.getListView().requestFocus();
                        mHandler.onKeyboardFocusChanged(getCurrentFocusedPosition());
                        return true;
                    } else if (KeyNavigationUtil.isEnter(event)) {
                        mHandler.onKeyboardActivatedItem(getCurrentFocusedPosition());
                    }
                }
                return false;
            }
        };

        View backIcon = iconRowView.findViewById(R.id.menu_item_back);
        backIcon.setEnabled(backMenuItem.isEnabled());
        backIcon.setFocusable(backMenuItem.isEnabled());
        backIcon.setOnKeyListener(keyListener);
        backIcon.setOnFocusChangeListener(focusListener);
        backIcon.setOnTouchListener(mDragScrollTouchEventForwarder);
        backIcon.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                dismiss();
                activity.onOptionsItemSelected(backMenuItem);
            }
        });

        View forwardIcon = iconRowView.findViewById(R.id.menu_item_forward);
        forwardIcon.setEnabled(forwardMenuItem.isEnabled());
        forwardIcon.setFocusable(forwardMenuItem.isEnabled());
        forwardIcon.setOnKeyListener(keyListener);
        forwardIcon.setOnFocusChangeListener(focusListener);
        forwardIcon.setOnTouchListener(mDragScrollTouchEventForwarder);
        forwardIcon.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                dismiss();
                activity.onOptionsItemSelected(forwardMenuItem);
            }
        });

        // The bookmark button is assumed to be always enabled and focusable when navigating the
        // menu using a keyboard.
        mBookmarkButton = (ImageButton) iconRowView.findViewById(R.id.menu_item_bookmark);
        mBookmarkButton.setEnabled(BookmarksBridge.isEditBookmarksEnabled());
        mBookmarkButton.setOnKeyListener(keyListener);
        mBookmarkButton.setOnFocusChangeListener(focusListener);
        mBookmarkButton.setOnTouchListener(mDragScrollTouchEventForwarder);
        mBookmarkButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                dismiss();
                activity.onOptionsItemSelected(bookmarkMenuItem);
            }
        });
    }

    /**
     * Dismisses the app menu and cancels the drag-to-scroll if it is taking place.
     */
    void dismiss() {
        mDragScrolling.cancel();
        if (isShowing()) {
            mPopup.dismiss();
        }
    }

    /**
     * @return Whether the app menu is currently showing.
     */
    public boolean isShowing() {
        if (mPopup == null) {
            return false;
        }
        return mPopup.isShowing();
    }

    private int getCurrentFocusedPosition() {
        if (mPopup == null || mPopup.getListView() == null) return ListView.INVALID_POSITION;
        ListView listView = mPopup.getListView();
        int position = listView.getSelectedItemPosition();

        // Check if any of the icon row icons are focused.
        if (mShowIconRow) {
            if (mIconRowView.findViewById(R.id.menu_item_back).isFocused() ||
                    mIconRowView.findViewById(R.id.menu_item_forward).isFocused() ||
                    mBookmarkButton.isFocused()) {
                return 0;
            }
        }
        return position;
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        // Account for header. MenuAdapter does not know about header,
        // but the 'position' includes the header.
        int adjustedPosition = mShowIconRow ? position - 1 : position;
        MenuItem clickedItem = mAdapter.getItem(adjustedPosition);
        if (clickedItem.isEnabled()) {
            dismiss();
            mActivity.onOptionsItemSelected(clickedItem);
        }
    }

    @VisibleForTesting
    int getCount() {
        if (mPopup == null || mPopup.getListView() == null) return 0;
        return mPopup.getListView().getCount();
    }

    /**
     * ListAdapter to customize the view of items in the list.
     */
    private static class MenuAdapter extends BaseAdapter {
        private static final int VIEW_TYPE_MENUITEM = 0;
        private static final int VIEW_TYPE_COUNT = 1;

        private final LayoutInflater mInflater;
        private final List<MenuItem> mMenuItems;
        private final int mNumMenuItems;

        public MenuAdapter(List<MenuItem> menuItems, LayoutInflater inflater) {
            mMenuItems = menuItems;
            mInflater = inflater;
            mNumMenuItems = menuItems.size();
        }

        @Override
        public int getCount() {
            return mNumMenuItems;
        }

        @Override
        public int getViewTypeCount() {
            return VIEW_TYPE_COUNT;
        }

        @Override
        public int getItemViewType(int position) {
            return VIEW_TYPE_MENUITEM;
        }

        @Override
        public long getItemId(int position) {
            return getItem(position).getItemId();
        }

        @Override
        public MenuItem getItem(int position) {
            if (position == ListView.INVALID_POSITION) return null;
            assert position >= 0;
            assert position < mMenuItems.size();
            return mMenuItems.get(position);
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View rowView = convertView;
            // A ViewHolder keeps references to children views to avoid unneccessary calls
            // to findViewById() on each row.
            ViewHolder holder = null;

            // When convertView is not null, we can reuse it directly, there is no need
            // to reinflate it.
            if (rowView == null) {
                holder = new ViewHolder();
                rowView = mInflater.inflate(R.layout.menu_item, null);
                holder.text = (TextView) rowView.findViewById(R.id.menu_item_text);
                holder.image = (MenuItemIcon) rowView.findViewById(R.id.menu_item_icon);
                rowView.setTag(holder);
            } else {
                holder = (ViewHolder) convertView.getTag();
            }
            MenuItem item = getItem(position);

            // Set up the icon.
            Drawable icon = item.getIcon();
            holder.image.setImageDrawable(icon);
            holder.image.setVisibility(icon == null ? View.GONE : View.VISIBLE);
            holder.image.setChecked(item.isChecked());

            holder.text.setText(item.getTitle());
            boolean isEnabled = item.isEnabled();
            // Set the text color (using a color state list).
            holder.text.setEnabled(isEnabled);
            // This will ensure that the item is not highlighted when selected.
            rowView.setEnabled(isEnabled);
            return rowView;
        }

        static class ViewHolder {
            TextView text;
            MenuItemIcon image;
        }
    }

    /**
     * A menu icon that supports the checkable state.
     */
    static class MenuItemIcon extends ImageView {
        private static final int[] CHECKED_STATE_SET = new int[] {android.R.attr.state_checked};
        private boolean mCheckedState;

        public MenuItemIcon(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        /**
         * Sets whether the item is checked and refreshes the View if necessary.
         */
        protected void setChecked(boolean state) {
            if (state == mCheckedState) return;
            mCheckedState = state;
            refreshDrawableState();
        }

        @Override
        public void setPressed(boolean state) {
            // We don't want to highlight the checkbox icon since the parent item is already
            // highlighted.
            return;
        }

        @Override
        public int[] onCreateDrawableState(int extraSpace) {
            final int[] drawableState = super.onCreateDrawableState(extraSpace + 1);
            if (mCheckedState) {
                mergeDrawableStates(drawableState, CHECKED_STATE_SET);
            }
            return drawableState;
        }
    }
}
