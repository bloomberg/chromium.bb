// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.annotation.SuppressLint;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.TypedArray;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.support.annotation.Nullable;
import android.view.ContextThemeWrapper;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.WindowManager;
import android.widget.PopupMenu;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;

import java.util.ArrayList;

/**
 * Object responsible for handling the creation, showing, hiding of the AppMenu and notifying the
 * AppMenuObservers about these actions.
 */
public class AppMenuHandler
        implements StartStopWithNativeObserver, OverviewModeBehavior.OverviewModeObserver {
    private AppMenu mAppMenu;
    private AppMenuDragHelper mAppMenuDragHelper;
    private Menu mMenu;
    private final ArrayList<AppMenuObserver> mObservers;
    private final int mMenuResourceId;
    private final View mHardwareButtonMenuAnchor;

    private final AppMenuPropertiesDelegate mDelegate;
    private final AppMenuCoordinator.AppMenuDelegate mAppMenuDelegate;
    private final View mDecorView;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final ComponentCallbacks mComponentCallbacks;
    private OverviewModeBehavior mOverviewModeBehavior;

    /**
     * The resource id of the menu item to highlight when the menu next opens. A value of
     * {@code null} means no item will be highlighted.  This value will be cleared after the menu is
     * opened.
     */
    private Integer mHighlightMenuId;

    /**
     *  Whether the highlighted item should use a circle highlight or not.
     */
    private boolean mCircleHighlight;

    /**
     * Constructs an AppMenuHandler object.
     * @param delegate Delegate used to check the desired AppMenu properties on show.
     * @param appMenuDelegate The AppMenuDelegate to handle menu item selection.
     * @param menuResourceId Resource Id that should be used as the source for the menu items.
     *            It is assumed to have back_menu_id, forward_menu_id, bookmark_this_page_id.
     * @param decorView The decor {@link View}, e.g. from Window#getDecorView(), for the containing
     *            activity.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the containing
     *            activity.
     */
    public AppMenuHandler(AppMenuPropertiesDelegate delegate,
            AppMenuCoordinator.AppMenuDelegate appMenuDelegate, int menuResourceId, View decorView,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mAppMenuDelegate = appMenuDelegate;
        mDelegate = delegate;
        mDecorView = decorView;
        mObservers = new ArrayList<>();
        mMenuResourceId = menuResourceId;
        mHardwareButtonMenuAnchor = mDecorView.findViewById(R.id.menu_anchor_stub);

        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);

        mComponentCallbacks = new ComponentCallbacks() {
            @Override
            public void onConfigurationChanged(Configuration configuration) {
                hideAppMenu();
            }

            @Override
            public void onLowMemory() {}
        };
        mDecorView.getContext().registerComponentCallbacks(mComponentCallbacks);

        assert mHardwareButtonMenuAnchor != null
                : "Using AppMenu requires to have menu_anchor_stub view";
    }

    /**
     * Called when the containing activity is being destroyed.
     */
    void destroy() {
        // Prevent the menu window from leaking.
        hideAppMenu();

        mActivityLifecycleDispatcher.unregister(this);
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(this);
        }

        mDecorView.getContext().unregisterComponentCallbacks(mComponentCallbacks);
    }

    /**
     * Called when native initialization has finished to provide additional activity-scoped objects
     * only available after native initialization.
     *
     * @param overviewModeBehavior The {@link OverviewModeBehavior} for the containing activity
     *         if the current activity supports an overview mode, or null otherwise.
     */
    void onNativeInitialized(@Nullable OverviewModeBehavior overviewModeBehavior) {
        if (overviewModeBehavior != null) {
            mOverviewModeBehavior = overviewModeBehavior;
            mOverviewModeBehavior.addOverviewModeObserver(this);
        }
    }

    /**
     * Notifies the menu that the contents of the menu item specified by {@code menuRowId} have
     * changed.  This should be called if icons, titles, etc. are changing for a particular menu
     * item while the menu is open.
     * @param menuRowId The id of the menu item to change.  This must be a row id and not a child
     *                  id.
     */
    public void menuItemContentChanged(int menuRowId) {
        if (mAppMenu != null) mAppMenu.menuItemContentChanged(menuRowId);
    }

    /**
     * Clears the menu highlight.
     */
    public void clearMenuHighlight() {
        setMenuHighlight(null, false);
    }

    /**
     * Calls attention to this menu and a particular item in it.  The menu will only stay
     * highlighted for one menu usage.  After that the highlight will be cleared.
     * @param highlightItemId The id of a menu item to highlight or {@code null} to turn off the
     *                        highlight.
     * @param circleHighlight Whether the highlighted item should use a circle highlight or not.
     */
    public void setMenuHighlight(Integer highlightItemId, boolean circleHighlight) {
        if (mHighlightMenuId == null && highlightItemId == null) return;
        if (mHighlightMenuId != null && mHighlightMenuId.equals(highlightItemId)) return;
        mHighlightMenuId = highlightItemId;
        mCircleHighlight = circleHighlight;
        boolean highlighting = mHighlightMenuId != null;
        for (AppMenuObserver observer : mObservers) observer.onMenuHighlightChanged(highlighting);
    }

    /**
     * Show the app menu.
     * @param anchorView    Anchor view (usually a menu button) to be used for the popup, if null is
     *                      passed then hardware menu button anchor will be used.
     * @param startDragging Whether dragging is started. For example, if the app menu is showed by
     *                      tapping on a button, this should be false. If it is showed by start
     *                      dragging down on the menu button, this should be true. Note that if
     *                      anchorView is null, this must be false since we no longer support
     *                      hardware menu button dragging.
     * @param showFromBottom Whether the menu should be shown from the bottom up.
     * @return              True, if the menu is shown, false, if menu is not shown, example
     *                      reasons: the menu is not yet available to be shown, or the menu is
     *                      already showing.
     */
    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint("ResourceType")
    @VisibleForTesting
    public boolean showAppMenu(View anchorView, boolean startDragging, boolean showFromBottom) {
        if (!mAppMenuDelegate.shouldShowAppMenu() || isAppMenuShowing()) return false;

        TextBubble.dismissBubbles();
        boolean isByPermanentButton = false;

        Context context = mDecorView.getContext();
        WindowManager wm = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        int rotation = wm.getDefaultDisplay().getRotation();
        if (anchorView == null) {
            // This fixes the bug where the bottom of the menu starts at the top of
            // the keyboard, instead of overlapping the keyboard as it should.
            int displayHeight = context.getResources().getDisplayMetrics().heightPixels;
            Rect rect = new Rect();
            mDecorView.getWindowVisibleDisplayFrame(rect);
            int statusBarHeight = rect.top;
            mHardwareButtonMenuAnchor.setY((displayHeight - statusBarHeight));

            anchorView = mHardwareButtonMenuAnchor;
            isByPermanentButton = true;
        }

        assert !(isByPermanentButton && startDragging);

        if (mMenu == null) {
            // Use a PopupMenu to create the Menu object. Note this is not the same as the
            // AppMenu (mAppMenu) created below.
            PopupMenu tempMenu = new PopupMenu(context, anchorView);
            tempMenu.inflate(mMenuResourceId);
            mMenu = tempMenu.getMenu();
        }
        mDelegate.prepareMenu(mMenu);

        ContextThemeWrapper wrapper =
                new ContextThemeWrapper(context, R.style.OverflowMenuThemeOverlay);

        if (mAppMenu == null) {
            TypedArray a = wrapper.obtainStyledAttributes(new int[]
                    {android.R.attr.listPreferredItemHeightSmall, android.R.attr.listDivider});
            int itemRowHeight = a.getDimensionPixelSize(0, 0);
            Drawable itemDivider = a.getDrawable(1);
            int itemDividerHeight = itemDivider != null ? itemDivider.getIntrinsicHeight() : 0;
            a.recycle();
            mAppMenu = new AppMenu(
                    mMenu, itemRowHeight, itemDividerHeight, this, context.getResources());
            mAppMenuDragHelper = new AppMenuDragHelper(context, mAppMenu, itemRowHeight);
        }

        // Get the height and width of the display.
        Rect appRect = new Rect();
        mDecorView.getWindowVisibleDisplayFrame(appRect);

        // Use full size of window for abnormal appRect.
        if (appRect.left < 0 && appRect.top < 0) {
            appRect.left = 0;
            appRect.top = 0;
            appRect.right = mDecorView.getWidth();
            appRect.bottom = mDecorView.getHeight();
        }
        Point pt = new Point();
        wm.getDefaultDisplay().getSize(pt);

        int footerResourceId = 0;
        if (mDelegate.shouldShowFooter(appRect.height())) {
            footerResourceId = mDelegate.getFooterResourceId();
        }
        int headerResourceId = 0;
        if (mDelegate.shouldShowHeader(appRect.height())) {
            headerResourceId = mDelegate.getHeaderResourceId();
        }
        mAppMenu.show(wrapper, anchorView, isByPermanentButton, rotation, appRect, pt.y,
                footerResourceId, headerResourceId, mHighlightMenuId, mCircleHighlight,
                showFromBottom);
        mAppMenuDragHelper.onShow(startDragging);
        clearMenuHighlight();
        RecordUserAction.record("MobileMenuShow");
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
    public AppMenu getAppMenu() {
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

    // StartStopWithNativeObserver implementation
    @Override
    public void onStartWithNative() {}

    @Override
    public void onStopWithNative() {
        hideAppMenu();
    }

    // OverviewModeBehavior.OverviewModeObserver implementation
    @Override
    public void onOverviewModeStartedShowing(boolean showToolbar) {
        hideAppMenu();
    }

    @Override
    public void onOverviewModeFinishedShowing() {}

    @Override
    public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
        hideAppMenu();
    }

    @Override
    public void onOverviewModeFinishedHiding() {}

    @VisibleForTesting
    public void onOptionsItemSelected(MenuItem item) {
        mAppMenuDelegate.onOptionsItemSelected(
                item.getItemId(), mDelegate.getBundleForMenuItem(item));
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

    /**
     * A notification that the header view has been inflated.
     * @param view The inflated view.
     */
    void onHeaderViewInflated(View view) {
        if (mDelegate != null) mDelegate.onHeaderViewInflated(mAppMenu, view);
    }

    /**
     * A notification that the footer view has been inflated.
     * @param view The inflated view.
     */
    void onFooterViewInflated(View view) {
        if (mDelegate != null) mDelegate.onFooterViewInflated(mAppMenu, view);
    }
}
