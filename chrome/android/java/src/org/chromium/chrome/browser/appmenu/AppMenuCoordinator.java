// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.content.Context;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewConfiguration;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.toolbar.ToolbarManager;

/** A UI coordinator the app menu. */
public class AppMenuCoordinator {
    /** A delegate to handle menu item selection. */
    public interface AppMenuDelegate {
        /**
         * Called whenever an item in the app menu is selected.
         * See {@link android.app.Activity#onOptionsItemSelected(MenuItem)}.
         * @param itemId The id of the menu item that was selected.
         * @param menuItemData Extra data associated with the menu item. May be null.
         */
        boolean onOptionsItemSelected(int itemId, @Nullable Bundle menuItemData);

        /**
         * @return {@link AppMenuPropertiesDelegate} instance that the {@link AppMenuHandler}
         *         should be using.
         */
        AppMenuPropertiesDelegate createAppMenuPropertiesDelegate();

        /**
         * @return Whether the app menu should be shown.
         */
        boolean shouldShowAppMenu();
    }

    /** A delegate that provides the button that triggers the app menu. */
    public interface MenuButtonDelegate {
        /**
         * @return The {@link View} for the menu button, used to anchor the app menu.
         */
        @Nullable
        View getMenuButtonView();

        /**
         * @return Whether the menu is shown from the bottom of the screen.
         */
        boolean isMenuFromBottom();
    }

    /**
     * Factory which creates the AppMenuHandler.
     */
    public interface AppMenuHandlerFactory {
        /**
         * @param delegate Delegate used to check the desired AppMenu properties on show.
         * @param appMenuDelegate The AppMenuDelegate to handle menu item selection.
         * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the
         *         containing activity.
         * @param menuResourceId Resource Id that should be used as the source for the menu items.
         *            It is assumed to have back_menu_id, forward_menu_id, bookmark_this_page_id.
         * @param decorView The decor {@link View}, e.g. from Window#getDecorView(), for the
         *         containing activity.
         * @return AppMenuHandler for the given activity and menu resource id.
         */
        AppMenuHandler get(AppMenuPropertiesDelegate delegate, AppMenuDelegate appMenuDelegate,
                int menuResourceId, View decorView,
                ActivityLifecycleDispatcher activityLifecycleDispatcher);
    }

    /**
     * @param factory The {@link AppMenuHandlerFactory} for creating {@link #mAppMenuHandler}
     */
    @VisibleForTesting
    public static void setAppMenuHandlerFactoryForTesting(AppMenuHandlerFactory factory) {
        sAppMenuHandlerFactory = factory;
    }

    private final Context mContext;
    private final MenuButtonDelegate mButtonDelegate;
    private final AppMenuDelegate mAppMenuDelegate;

    private AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    private AppMenuHandler mAppMenuHandler;

    private static AppMenuHandlerFactory sAppMenuHandlerFactory =
            (delegate, appMenuDelegate, menuResourceId, decorView, activityLifecycleDispatcher)
            -> new AppMenuHandler(delegate, appMenuDelegate, menuResourceId, decorView,
                    activityLifecycleDispatcher);

    /**
     * Construct a new AppMenuCoordinator.
     * @param context The activity context.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the containing
     *         activity.
     * @param buttonDelegate The {@link ToolbarManager} for the containing activity.
     * @param appMenuDelegate The {@link AppMenuDelegate} for the containing activity.
     * @param decorView The decor {@link View}, e.g. from Window#getDecorView(), for the containing
     *         activity.
     */
    public AppMenuCoordinator(Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            MenuButtonDelegate buttonDelegate, AppMenuDelegate appMenuDelegate, View decorView) {
        mContext = context;
        mButtonDelegate = buttonDelegate;
        mAppMenuDelegate = appMenuDelegate;
        mAppMenuPropertiesDelegate = mAppMenuDelegate.createAppMenuPropertiesDelegate();

        mAppMenuHandler = sAppMenuHandlerFactory.get(mAppMenuPropertiesDelegate, mAppMenuDelegate,
                mAppMenuPropertiesDelegate.getAppMenuLayoutId(), decorView,
                activityLifecycleDispatcher);

        // TODO(twellington): Move to UpdateMenuItemHelper or common UI coordinator parent?
        mAppMenuHandler.addObserver(new AppMenuObserver() {
            @Override
            public void onMenuVisibilityChanged(boolean isVisible) {
                if (isVisible) return;

                mAppMenuPropertiesDelegate.onMenuDismissed();
                MenuItem updateMenuItem =
                        mAppMenuHandler.getAppMenu().getMenu().findItem(R.id.update_menu_id);
                if (updateMenuItem != null && updateMenuItem.isVisible()) {
                    UpdateMenuItemHelper.getInstance().onMenuDismissed();
                }
            }

            @Override
            public void onMenuHighlightChanged(boolean highlighting) {}
        });
    }

    /**
     * Called when the containing activity is being destroyed.
     */
    public void destroy() {
        // Prevent the menu window from leaking.
        if (mAppMenuHandler != null) mAppMenuHandler.destroy();
    }

    /**
     * Called when native initialization has finished to provide additional activity-scoped objects
     * only available after native initialization.
     *
     * @param overviewModeBehavior The {@link OverviewModeBehavior} for the containing activity
     *         if the current activity supports an overview mode, or null otherwise.
     */
    public void onNativeInitialized(@Nullable OverviewModeBehavior overviewModeBehavior) {
        // TODO(https://crbug.com/956260): Look into providing this during construction
        // (e.g. through an AsyncSupplier that can notify when the dependency is available).
        mAppMenuPropertiesDelegate.onNativeInitialized(overviewModeBehavior);
        mAppMenuHandler.onNativeInitialized(overviewModeBehavior);
    }

    /**
     * Shows the app menu (if possible) for a key press on the keyboard with the correct anchor view
     * chosen depending on device configuration and the visible menu button to the user.
     */
    public void showAppMenuForKeyboardEvent() {
        if (mAppMenuHandler == null || !mAppMenuDelegate.shouldShowAppMenu()) return;

        boolean hasPermanentMenuKey = ViewConfiguration.get(mContext).hasPermanentMenuKey();
        mAppMenuHandler.showAppMenu(
                hasPermanentMenuKey ? null : mButtonDelegate.getMenuButtonView(), false,
                mButtonDelegate.isMenuFromBottom());
    }

    /**
     * @return The {@link AppMenuHandler} associated with this activity.
     */
    public AppMenuHandler getAppMenuHandler() {
        return mAppMenuHandler;
    }

    /**
     * @return The {@link AppMenuPropertiesDelegate} associated with this activity.
     */
    public AppMenuPropertiesDelegate getAppMenuPropertiesDelegate() {
        return mAppMenuPropertiesDelegate;
    }
}
