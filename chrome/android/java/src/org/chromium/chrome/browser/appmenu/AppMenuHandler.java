// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.app.Activity;
import android.content.res.TypedArray;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.PopupMenu;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.UmaBridge;

import java.util.ArrayList;

/**
 * Object responsible for handling the creation, showing, hiding of the AppMenu and notifying the
 * AppMenuObservers about these actions.
 */
public class AppMenuHandler {
    private AppMenu mAppMenu;
    private AppMenuDragHelper mAppMenuDragHelper;
    private Menu mMenu;
    private final ArrayList<AppMenuObserver> mObservers;
    private final int mMenuResourceId;

    private final AppMenuPropertiesDelegate mDelegate;
    private final Activity mActivity;

    /**
     * Constructs an AppMenuHandler object.
     * @param activity Activity that is using the AppMenu.
     * @param delegate Delegate used to check the desired AppMenu properties on show.
     * @param menuResourceId Resource Id that should be used as the source for the menu items.
     *            It is assumed to have back_menu_id, forward_menu_id, bookmark_this_page_id.
     */
    public AppMenuHandler(Activity activity, AppMenuPropertiesDelegate delegate,
            int menuResourceId) {
        mActivity = activity;
        mDelegate = delegate;
        mObservers = new ArrayList<AppMenuObserver>();
        mMenuResourceId = menuResourceId;
    }

    /**
     * Show the app menu.
     * @param anchorView         Anchor view (usually a menu button) to be used for the popup.
     * @param isByHardwareButton True if hardware button triggered it. (oppose to software
     *                           button)
     * @param startDragging      Whether dragging is started. For example, if the app menu is
     *                           showed by tapping on a button, this should be false. If it is
     *                           showed by start dragging down on the menu button, this should
     *                           be true. Note that if isByHardwareButton is true, this must
     *                           be false since we no longer support hardware menu button
     *                           dragging.
     * @return True, if the menu is shown, false, if menu is not shown, example reasons:
     *         the menu is not yet available to be shown, or the menu is already showing.
     */
    public boolean showAppMenu(View anchorView, boolean isByHardwareButton, boolean startDragging) {
        assert !(isByHardwareButton && startDragging);
        if (!mDelegate.shouldShowAppMenu() || isAppMenuShowing()) return false;

        if (mMenu == null) {
            // Use a PopupMenu to create the Menu object. Note this is not the same as the
            // AppMenu (mAppMenu) created below.
            PopupMenu tempMenu = new PopupMenu(mActivity, anchorView);
            tempMenu.inflate(mMenuResourceId);
            mMenu = tempMenu.getMenu();
        }
        mDelegate.prepareMenu(mMenu);

        ContextThemeWrapper wrapper = new ContextThemeWrapper(mActivity,
                mDelegate.getMenuThemeResourceId());

        if (mAppMenu == null) {
            TypedArray a = wrapper.obtainStyledAttributes(new int[]
                    {android.R.attr.listPreferredItemHeightSmall, android.R.attr.listDivider});
            int itemRowHeight = a.getDimensionPixelSize(0, 0);
            Drawable itemDivider = a.getDrawable(1);
            int itemDividerHeight = itemDivider != null ? itemDivider.getIntrinsicHeight() : 0;
            a.recycle();
            mAppMenu = new AppMenu(mMenu, itemRowHeight, itemDividerHeight, this,
                    mActivity.getResources());
            mAppMenuDragHelper = new AppMenuDragHelper(mActivity, mAppMenu, itemRowHeight);
        }

        // Get the height and width of the display.
        Rect appRect = new Rect();
        mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(appRect);

        // Use full size of window for abnormal appRect.
        if (appRect.left < 0 && appRect.top < 0) {
            appRect.left = 0;
            appRect.top = 0;
            appRect.right = mActivity.getWindow().getDecorView().getWidth();
            appRect.bottom = mActivity.getWindow().getDecorView().getHeight();
        }
        int rotation = mActivity.getWindowManager().getDefaultDisplay().getRotation();
        Point pt = new Point();
        mActivity.getWindowManager().getDefaultDisplay().getSize(pt);
        mAppMenu.show(wrapper, anchorView, isByHardwareButton, rotation, appRect, pt.y);
        mAppMenuDragHelper.onShow(startDragging);
        UmaBridge.menuShow();
        return true;
    }

    void appMenuDismissed() {
        mAppMenuDragHelper.finishDragging();
    }

    /**
     * @return Whether the App Menu is currently showing.
     */
    public boolean isAppMenuShowing() {
        return mAppMenu != null && mAppMenu.isShowing();
    }

    /**
     * @return The App Menu that the menu handler is interacting with.
     */
    @VisibleForTesting
    AppMenu getAppMenu() {
        return mAppMenu;
    }

    AppMenuDragHelper getAppMenuDragHelper() {
        return mAppMenuDragHelper;
    }

    /**
     * Requests to hide the App Menu.
     */
    public void hideAppMenu() {
        if (mAppMenu != null && mAppMenu.isShowing()) mAppMenu.dismiss();
    }

    /**
     * Adds the observer to App Menu.
     * @param observer Observer that should be notified about App Menu changes.
     */
    public void addObserver(AppMenuObserver observer) {
        mObservers.add(observer);
    }

    /**
     * Removes the observer from the App Menu.
     * @param observer Observer that should no longer be notified about App Menu changes.
     */
    public void removeObserver(AppMenuObserver observer) {
        mObservers.remove(observer);
    }

    void onOptionsItemSelected(MenuItem item) {
        mActivity.onOptionsItemSelected(item);
    }

    /**
     * Called by AppMenu to report that the App Menu visibility has changed.
     * @param isVisible Whether the App Menu is showing.
     */
    void onMenuVisibilityChanged(boolean isVisible) {
        for (int i = 0; i < mObservers.size(); ++i) {
            mObservers.get(i).onMenuVisibilityChanged(isVisible);
        }
    }
}
