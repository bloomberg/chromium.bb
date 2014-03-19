// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.Surface;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnKeyListener;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.HeaderViewListAdapter;
import android.widget.ImageButton;
import android.widget.ListPopupWindow;
import android.widget.ListView;
import android.widget.ListView.FixedViewInfo;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;

import com.google.common.annotations.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.BookmarksBridge;
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
    private static final float LAST_ITEM_SHOW_FRACTION = 0.5f;
    private static final int DIVIDER_HEIGHT_DP = 1;

    private final Menu mMenu;
    private final Activity mActivity;
    private final int mItemRowHeight;
    private final int mItemDividerHeight;
    private final int mVerticalFadeDistance;
    private final int mAdditionalVerticalOffset;
    private ListPopupWindow mPopup;
    private LayoutInflater mInflater;
    private boolean mShowIconRow;
    private AppMenuAdapter mAdapter;
    private View mIconRowView;
    private AppMenuHandler mHandler;
    private int mCurrentScreenRotation = -1;
    private boolean mIsByHardwareButton;

    /**
     * Creates and sets up the App Menu.
     * @param activity Activity that will handle app menu callbacks.
     * @param menu Original menu created by the framework.
     * @param itemRowHeight Desired height for each app menu row.
     */
    AppMenu(Activity activity, Menu menu, int itemRowHeight, AppMenuHandler handler) {
        mActivity = activity;
        mMenu = menu;

        mItemRowHeight = itemRowHeight;
        assert mItemRowHeight > 0;

        mHandler = handler;
        Resources res = mActivity.getResources();

        final float dpToPx = res.getDisplayMetrics().density;
        mItemDividerHeight = (int) (DIVIDER_HEIGHT_DP * dpToPx);

        mAdditionalVerticalOffset = res.getDimensionPixelSize(R.dimen.menu_vertical_offset);
        mVerticalFadeDistance = res.getDimensionPixelSize(R.dimen.menu_vertical_fade_distance);
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
     */
    void show(Context context, View anchorView, boolean showIconRow, boolean isByHardwareButton) {
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
                mHandler.onMenuVisibilityChanged(false);
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
        mAdapter = new AppMenuAdapter(menuItems, mInflater);
        if (mShowIconRow) {
            mIconRowView = mInflater.inflate(R.layout.menu_icon_row, null);
            // Add click handlers for the header icons.
            setIconRowEventHandlers(mIconRowView);
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

        mHandler.onMenuVisibilityChanged(true);

        if (mVerticalFadeDistance > 0) {
            mPopup.getListView().setVerticalFadingEdgeEnabled(true);
            mPopup.getListView().setFadingEdgeLength(mVerticalFadeDistance);
        }

        mPopup.getListView().setOnKeyListener(this);
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
            return handled;
        } else if (KeyNavigationUtil.isGoDown(event)) {
            boolean handled = mPopup.onKeyDown(keyCode, event);
            if (listView.getSelectedItemPosition() == ListView.INVALID_POSITION) {
                listView.setSelection(listView.getCount() - 1);
            }
            return handled;
        } else if (KeyNavigationUtil.isEnter(event)) {
            int position = getCurrentFocusedPosition();
            if (mShowIconRow && position == 0) return false;

            int adjustedPosition = mShowIconRow ? position - 1 : position;
            MenuItem clickedItem = mAdapter.getItem(adjustedPosition);
            dismiss();
            mHandler.onOptionsItemSelected(clickedItem);
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

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        // Account for header. MenuAdapter does not know about header,
        // but the 'position' includes the header.
        int adjustedPosition = mShowIconRow ? position - 1 : position;
        MenuItem clickedItem = mAdapter.getItem(adjustedPosition);
        if (clickedItem.isEnabled()) {
            dismiss();
            mHandler.onOptionsItemSelected(clickedItem);
        }
    }

    /**
     * Dismisses the app menu and cancels the drag-to-scroll if it is taking place.
     */
    void dismiss() {
        mHandler.appMenudismiss();
        if (isShowing()) mPopup.dismiss();
    }

    /**
     * @return Whether the app menu is currently showing.
     */
    boolean isShowing() {
        if (mPopup == null) {
            return false;
        }
        return mPopup.isShowing();
    }

    @VisibleForTesting
    int getCount() {
        if (mPopup == null || mPopup.getListView() == null) return 0;
        return mPopup.getListView().getCount();
    }

    ListPopupWindow getPopup() {
        return mPopup;
    }

    boolean isShowingIconRow() {
        return mShowIconRow;
    }

    View getIconRowView() {
        return mIconRowView;
    }

    int getItemRowHeight() {
        return mItemRowHeight;
    }

    private void updateBookmarkButton() {
        final MenuItem bookmarkMenuItem = mMenu.findItem(R.id.bookmark_this_page_id);
        ImageButton bookmarkButton =
                (ImageButton) mIconRowView.findViewById(R.id.menu_item_bookmark);
        if (bookmarkMenuItem.isEnabled()) {
            bookmarkButton.setImageResource(R.drawable.star);
            bookmarkButton.setContentDescription(bookmarkButton.getContext().getString(
                    R.string.accessibility_menu_bookmark));
        } else {
            bookmarkButton.setImageResource(R.drawable.star_lit);
            bookmarkButton.setContentDescription(bookmarkButton.getContext().getString(
                    R.string.accessibility_menu_edit_bookmark));
        }
    }

    @VisibleForTesting
    int getCurrentFocusedPosition() {
        if (mPopup == null || mPopup.getListView() == null) return ListView.INVALID_POSITION;
        ListView listView = mPopup.getListView();
        int position = listView.getSelectedItemPosition();

        // Check if any of the icon row icons are focused.
        if (mShowIconRow) {
            if (mIconRowView.findViewById(R.id.menu_item_back).isFocused() ||
                    mIconRowView.findViewById(R.id.menu_item_forward).isFocused() ||
                    mIconRowView.findViewById(R.id.menu_item_bookmark).isFocused()) {
                return 0;
            }
        }
        return position;
    }

    @VisibleForTesting
    int getCurrentFocusedItemId() {
        int position = getCurrentFocusedPosition();
        if (position == ListView.INVALID_POSITION) return -1;
        if (mShowIconRow && position == 0) {
            if (mIconRowView.findViewById(R.id.menu_item_back).isFocused()) {
                return R.id.back_menu_id;
            } else if (mIconRowView.findViewById(R.id.menu_item_forward).isFocused()) {
                return R.id.forward_menu_id;
            } else if (mIconRowView.findViewById(R.id.menu_item_bookmark).isFocused()) {
                return R.id.bookmark_this_page_id;
            }
        }
        int adjustedPosition = mShowIconRow ? position - 1 : position;
        return mAdapter.getItem(adjustedPosition).getItemId();
    }

    /**
     * Adds click handlers for items in the icon row.
     * Also disable/enable the view based on the menu item.
     * We assume that we have Back, Forward and Bookmark-star icons in this view.
     */
    private void setIconRowEventHandlers(View iconRowView) {
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
                        return true;
                    } else if (KeyNavigationUtil.isGoDown(event)) {
                        // Requesting focus on the mPopup.getListView().getChildAt(0) does not work.
                        // Requesting the focus on the ListView focuses the first non-header item.
                        mPopup.getListView().requestFocus();
                        return true;
                    }
                }
                return false;
            }
        };

        MenuItem menuItem = mMenu.findItem(R.id.back_menu_id);
        setIconRowItemEventHandlers(menuItem, iconRowView.findViewById(R.id.menu_item_back),
                keyListener, menuItem.isEnabled());

        menuItem = mMenu.findItem(R.id.forward_menu_id);
        setIconRowItemEventHandlers(menuItem, iconRowView.findViewById(R.id.menu_item_forward),
                keyListener, menuItem.isEnabled());

        menuItem = mMenu.findItem(R.id.bookmark_this_page_id);
        setIconRowItemEventHandlers(menuItem, iconRowView.findViewById(R.id.menu_item_bookmark),
                keyListener, BookmarksBridge.isEditBookmarksEnabled());
    }

    private void setIconRowItemEventHandlers(final MenuItem menuItem, View iconView,
            OnKeyListener keyListener, boolean enabled) {
        iconView.setEnabled(enabled);
        iconView.setFocusable(enabled);
        iconView.setOnKeyListener(keyListener);
        iconView.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                dismiss();
                mHandler.onOptionsItemSelected(menuItem);
            }
        });
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
}