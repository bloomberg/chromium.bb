// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.Surface;
import android.view.View;
import android.view.View.OnKeyListener;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ImageButton;
import android.widget.ListPopupWindow;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;

import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.List;

/**
 * Shows a popup of menuitems anchored to a host view. When a item is selected we call
 * Activity.onOptionsItemSelected with the appropriate MenuItem.
 *   - Only visible MenuItems are shown.
 *   - Disabled items are grayed out.
 */
public class AppMenu implements OnItemClickListener, OnKeyListener {
    /** Whether or not to show the software menu button in the menu. */
    private static final boolean SHOW_SW_MENU_BUTTON = true;

    private static final float LAST_ITEM_SHOW_FRACTION = 0.5f;

    private final Menu mMenu;
    private final int mItemRowHeight;
    private final int mItemDividerHeight;
    private final int mVerticalFadeDistance;
    private ListPopupWindow mPopup;
    private AppMenuAdapter mAdapter;
    private AppMenuHandler mHandler;
    private int mCurrentScreenRotation = -1;
    private boolean mIsByHardwareButton;

    /**
     * Creates and sets up the App Menu.
     * @param menu Original menu created by the framework.
     * @param itemRowHeight Desired height for each app menu row.
     * @param itemDividerHeight Desired height for the divider between app menu items.
     * @param handler AppMenuHandler receives callbacks from AppMenu.
     * @param res Resources object used to get dimensions and style attributes.
     */
    AppMenu(Menu menu, int itemRowHeight, int itemDividerHeight, AppMenuHandler handler,
            Resources res) {
        mMenu = menu;

        mItemRowHeight = itemRowHeight;
        assert mItemRowHeight > 0;

        mHandler = handler;

        mItemDividerHeight = itemDividerHeight;
        assert mItemDividerHeight >= 0;

        mVerticalFadeDistance = res.getDimensionPixelSize(R.dimen.menu_vertical_fade_distance);
    }

    /**
     * Creates and shows the app menu anchored to the specified view.
     *
     * @param context             The context of the AppMenu (ensure the proper theme is set on
     *                            this context).
     * @param anchorView          The anchor {@link View} of the {@link ListPopupWindow}.
     * @param isByHardwareButton  Whether or not hardware button triggered it. (oppose to software
     *                            button)
     * @param screenRotation      Current device screen rotation.
     * @param visibleDisplayFrame The display area rect in which AppMenu is supposed to fit in.
     */
    void show(Context context, View anchorView, boolean isByHardwareButton, int screenRotation,
            Rect visibleDisplayFrame) {
        mPopup = new ListPopupWindow(context, null, android.R.attr.popupMenuStyle);
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

        // Need to explicitly set the background here.  Relying on it being set in the style caused
        // an incorrectly drawn background.
        if (isByHardwareButton) {
            mPopup.setBackgroundDrawable(context.getResources().getDrawable(R.drawable.menu_bg));
        } else {
            mPopup.setBackgroundDrawable(
                    context.getResources().getDrawable(R.drawable.edge_menu_bg));
            mPopup.setAnimationStyle(R.style.OverflowMenuAnim);
        }

        Rect bgPadding = new Rect();
        mPopup.getBackground().getPadding(bgPadding);

        int popupWidth = context.getResources().getDimensionPixelSize(R.dimen.menu_width) +
                bgPadding.left + bgPadding.right;

        mPopup.setWidth(popupWidth);

        mCurrentScreenRotation = screenRotation;
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

        boolean showMenuButton = !mIsByHardwareButton;
        if (!SHOW_SW_MENU_BUTTON) showMenuButton = false;
        // A List adapter for visible items in the Menu. The first row is added as a header to the
        // list view.
        mAdapter = new AppMenuAdapter(
                this, menuItems, LayoutInflater.from(context), showMenuButton);
        mPopup.setAdapter(mAdapter);

        setMenuHeight(menuItems.size(), visibleDisplayFrame);
        setPopupOffset(mPopup, mCurrentScreenRotation, visibleDisplayFrame);
        mPopup.setOnItemClickListener(this);
        mPopup.show();
        mPopup.getListView().setItemsCanFocus(true);
        mPopup.getListView().setOnKeyListener(this);

        mHandler.onMenuVisibilityChanged(true);

        if (mVerticalFadeDistance > 0) {
            mPopup.getListView().setVerticalFadingEdgeEnabled(true);
            mPopup.getListView().setFadingEdgeLength(mVerticalFadeDistance);
        }

        mPopup.getListView().addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                mPopup.getListView().removeOnLayoutChangeListener(this);
                runMenuItemEnterAnimations();
            }
        });
    }

    private void setPopupOffset(ListPopupWindow popup, int screenRotation, Rect appRect) {
        Rect paddingRect = new Rect();
        popup.getBackground().getPadding(paddingRect);
        int[] anchorLocation = new int[2];
        popup.getAnchorView().getLocationInWindow(anchorLocation);
        int anchorHeight = popup.getAnchorView().getHeight();

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
            popup.setVerticalOffset(-paddingRect.bottom);
        } else {
            // The menu is displayed over and below the anchored view, so shift the menu up by the
            // height of the anchor view.
            popup.setVerticalOffset(-anchorHeight);
        }
    }

    /**
     * Handles clicks on the AppMenu popup.
     * @param menuItem The menu item in the popup that was clicked.
     */
    void onItemClick(MenuItem menuItem) {
        if (menuItem.isEnabled()) {
            dismiss();
            mHandler.onOptionsItemSelected(menuItem);
        }
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        onItemClick(mAdapter.getItem(position));
    }

    @Override
    public boolean onKey(View v, int keyCode, KeyEvent event) {
        if (mPopup == null || mPopup.getListView() == null) return false;

        if (event.getKeyCode() == KeyEvent.KEYCODE_MENU) {
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
        }
        return false;
    }

    /**
     * Dismisses the app menu and cancels the drag-to-scroll if it is taking place.
     */
    void dismiss() {
        mHandler.appMenuDismissed();
        if (isShowing()) {
            mPopup.dismiss();
        }
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

    /**
     * @return ListPopupWindow that displays all the menu options.
     */
    ListPopupWindow getPopup() {
        return mPopup;
    }

    private void setMenuHeight(int numMenuItems, Rect appDimensions) {
        assert mPopup.getAnchorView() != null;
        View anchorView = mPopup.getAnchorView();
        int[] anchorViewLocation = new int[2];
        anchorView.getLocationOnScreen(anchorViewLocation);
        anchorViewLocation[1] -= appDimensions.top;
        int anchorViewImpactHeight = mIsByHardwareButton ? anchorView.getHeight() : 0;

        int availableScreenSpace = Math.max(anchorViewLocation[1],
                appDimensions.height() - anchorViewLocation[1] - anchorViewImpactHeight);

        Rect padding = new Rect();
        mPopup.getBackground().getPadding(padding);
        availableScreenSpace -= mIsByHardwareButton ? padding.top : padding.bottom;

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

    private void runMenuItemEnterAnimations() {
        AnimatorSet animation = new AnimatorSet();
        AnimatorSet.Builder builder = null;

        ViewGroup list = mPopup.getListView();
        for (int i = 0; i < list.getChildCount(); i++) {
            View view = list.getChildAt(i);
            Object animatorObject = view.getTag(R.id.menu_item_enter_anim_id);
            if (animatorObject != null) {
                if (builder == null) {
                    builder = animation.play((Animator) animatorObject);
                } else {
                    builder.with((Animator) animatorObject);
                }
            }
        }

        animation.start();
    }
}
