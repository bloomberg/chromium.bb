// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.graphics.Bitmap;
import android.os.SystemClock;
import android.support.v7.app.ActionBar;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.FrameLayout;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ContextualMenuBar;
import org.chromium.chrome.browser.CustomSelectionActionModeCallback;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabObserver;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.appmenu.ChromeAppMenuPropertiesDelegate;
import org.chromium.chrome.browser.compositor.Invalidator;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.toolbar.ToolbarManager.MenuDelegatePhone;
import org.chromium.chrome.browser.widget.ControlContainer;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarManager;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeUnit;

/**
 * Helper class to handle inflating the right view hierarchy and initialize it.
 */
public class ToolbarHelper {
    /**
     * The number of ms to wait before reporting to UMA omnibox interaction metrics.
     */
    private static final int RECORD_UMA_PERFORMANCE_METRICS_DELAY_MS = 30000;

    private static final int MIN_FOCUS_TIME_FOR_UMA_HISTOGRAM_MS = 1000;
    private static final int MAX_FOCUS_TIME_FOR_UMA_HISTOGRAM_MS = 30000;

    protected final ChromeActivity mActivity;
    private final ToolbarControlContainer mControlContainer;
    private TabObserver mTabObserver;

    private final Toolbar mToolbar;
    private final ToolbarManager mToolbarManager;

    private final ContextualMenuBar mContextualMenuBar;
    private final ContextualMenuBar.ActionBarDelegate mActionBarDelegate;

    private boolean mInitialized;

    /**
     * Constructor for {@link ToolbarHelper}.
     * @param activity The current activity.
     * @param controlContainer {@link ControlContainer} that the Toolbar View Hierarchy will be
     *        attached to.
     * @param appMenuHandler Controller for showing/hiding app menu.
     * @param appMenuPropertiesDelegate Controller for app menu item visibility.
     * @param invalidator Notifier to toolbar to force view invalidations.
     */
    public ToolbarHelper(ChromeActivity activity,
            ToolbarControlContainer controlContainer,
            final AppMenuHandler appMenuHandler,
            final ChromeAppMenuPropertiesDelegate appMenuPropertiesDelegate,
            Invalidator invalidator) {
        mActivity = activity;
        mControlContainer = controlContainer;
        assert mControlContainer != null;

        mToolbarManager = new ToolbarManager(mControlContainer, appMenuHandler);
        mToolbar = mToolbarManager.getToolbar();

        mActionBarDelegate = new ContextualMenuBar.ActionBarDelegate() {
            @Override
            public void setControlTopMargin(int margin) {
                FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams)
                        mControlContainer.getLayoutParams();
                lp.topMargin = margin;
                mControlContainer.setLayoutParams(lp);
            }

            @Override
            public int getControlTopMargin() {
                FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams)
                        mControlContainer.getLayoutParams();
                return lp.topMargin;
            }

            @Override
            public ActionBar getSupportActionBar() {
                return mActivity.getSupportActionBar();
            }

            @Override
            public void setActionBarBackgroundVisibility(boolean visible) {
                int visibility = visible ? View.VISIBLE : View.GONE;
                mActivity.findViewById(R.id.action_bar_black_background).setVisibility(visibility);
                // TODO(tedchoc): Add support for changing the color based on the brand color.
            }
        };
        mContextualMenuBar = new ContextualMenuBar(activity, mActionBarDelegate);
        mContextualMenuBar.setCustomSelectionActionModeCallback(
                new CustomSelectionActionModeCallback());

        mToolbar.setPaintInvalidator(invalidator);
        mToolbarManager.setDefaultActionModeCallbackForTextEdit(
                mContextualMenuBar.getCustomSelectionActionModeCallback());
        mToolbar.getLocationBar().initializeControls(
                new WindowDelegate(mActivity.getWindow()),
                mContextualMenuBar.getActionBarDelegate(),
                mActivity.getWindowAndroid());
        mToolbar.getLocationBar().setIgnoreURLBarModification(false);

        MenuDelegatePhone menuDelegate = new MenuDelegatePhone() {
            @Override
            public void updateReloadButtonState(boolean isLoading) {
                if (appMenuPropertiesDelegate != null) {
                    appMenuPropertiesDelegate.loadingStateChanged(isLoading);
                    appMenuHandler.menuItemContentChanged(R.id.icon_row_menu_id);
                }
            }
        };
        mToolbarManager.setMenuDelegatePhone(menuDelegate);
    }

    /**
     * Initializes the controls for the current toolbar mode.
     * @param findToolbarManager Manager for find in page.
     * @param overviewModeBehavior Interface to query and listen to the overview state.
     * @param layoutManager A {@link LayoutManager} instance to use to watch for scene changes.
     * @param tabSwitcherClickHandler Click listener for the tab switcher button.
     * @param newTabClickHandler Click listener for the new tab button.
     * @param bookmarkClickHandler Click listener for the bookmark button.
     */
    public void initializeControls(FindToolbarManager findToolbarManager,
            OverviewModeBehavior overviewModeBehavior,
            LayoutManager layoutManager,
            OnClickListener tabSwitcherClickHandler,
            OnClickListener newTabClickHandler,
            OnClickListener bookmarkClickHandler,
            OnClickListener customTabsBackClickHandler) {


        TabModelSelector selector = mActivity.getTabModelSelector();
        mTabObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onContextualActionBarVisibilityChanged(Tab tab, boolean visible) {
                if (visible) RecordUserAction.record("MobileActionBarShown");
                ActionBar actionBar = mActionBarDelegate.getSupportActionBar();
                if (!visible && actionBar != null) actionBar.hide();
                if (DeviceFormFactor.isTablet(mActivity)) {
                    if (visible) {
                        mContextualMenuBar.showControls();
                    } else {
                        mContextualMenuBar.hideControls();
                    }
                }
            }
        };
        mToolbarManager.initializeWithNative(
                selector, mActivity.getFullscreenManager(),
                findToolbarManager, overviewModeBehavior, layoutManager);
        mToolbar.getLocationBar().updateVisualsForState();
        mToolbar.getLocationBar().setUrlToPageUrl();
        mToolbar.setOnTabSwitcherClickHandler(tabSwitcherClickHandler);
        mToolbar.setOnNewTabClickHandler(newTabClickHandler);
        mToolbar.setBookmarkClickHandler(bookmarkClickHandler);
        mToolbar.setCustomTabReturnClickHandler(customTabsBackClickHandler);
        mInitialized = true;
    }

    /**
     * @return The menu bar for handling contextual text selection.
     */
    public ContextualMenuBar getContextualMenuBar() {
        return mContextualMenuBar;
    }

    /**
     * @return Whether the UI has been initialized.
     */
    public boolean isInitialized() {
        return mInitialized;
    }

    /**
     * @return The view that the pop up menu should be anchored to on the UI.
     */
    public View getMenuAnchor() {
        return mToolbar.getLocationBar().getMenuAnchor();
    }

    /**
     * Should be called when the accessibility state changes.
     * @param enabled Whether or not accessibility and touch exploration are enabled.
     */
    public void onAccessibilityStatusChanged(boolean enabled) {
        mToolbarManager.onAccessibilityStatusChanged(enabled);
    }

    /**
     * Update the brand color for the titlebar.
     * @param color The current theme color used.
     */
    public void setThemeColor(int color) {
        mToolbarManager.updatePrimaryColor(color);
    }

    /**
     * Adds a custom action button to the {@link Toolbar} if it is supported.
     * @param buttonSource The {@link Bitmap} resource to use as the source for the button.
     * @param listener The {@link OnClickListener} to use for clicks to the button.
     */
    public void addCustomActionButton(Bitmap buttonSource, OnClickListener listener) {
        mToolbar.addCustomActionButton(buttonSource, listener);
    }

    /**
     * Call to tear down all of the toolbar dependencies.
     */
    public void destroy() {
        Tab activityTab = mActivity.getActivityTab();
        if (activityTab != null) activityTab.removeObserver(mTabObserver);
    }

    /**
     * Called when the orientation of the activity has changed.
     */
    public void onOrientationChange() {
        mContextualMenuBar.showControlsOnOrientationChange();
    }

    /**
     * Navigates the current Tab back.
     * @return Whether or not the current Tab did go back.
     */
    public boolean back() {
        return mToolbarManager.back();
    }

    /**
     * Focuses or unfocuses the URL bar.
     * @param focused Whether URL bar should be focused.
     */
    public void setUrlBarFocus(boolean focused) {
        if (!isInitialized()) return;
        mToolbar.getLocationBar().setUrlBarFocus(focused);
    }

    /**
     * @return Whether {@link Toolbar} has drawn at least once.
     */
    public boolean hasDoneFirstDraw() {
        return mToolbar.getFirstDrawTime() != 0;
    }

    /**
     * Handle all necessary tasks that can be delayed until initialization completes.
     */
    public void onDeferredStartup() {
        // Record startup performance statistics
        long elapsedTime = SystemClock.elapsedRealtime() - mActivity.getOnCreateTimestampMs();
        if (elapsedTime < RECORD_UMA_PERFORMANCE_METRICS_DELAY_MS) {
            ThreadUtils.postOnUiThreadDelayed(new Runnable() {
                @Override
                public void run() {
                    onDeferredStartup();
                }
            }, RECORD_UMA_PERFORMANCE_METRICS_DELAY_MS - elapsedTime);
        }

        long creationTime = mActivity.getOnCreateTimestampMs();
        String className = mActivity.getClass().getSimpleName();
        RecordHistogram.recordTimesHistogram("MobileStartup.ToolbarFirstDrawTime." + className,
                mToolbar.getFirstDrawTime() - creationTime, TimeUnit.MILLISECONDS);

        long firstFocusTime = mToolbar.getLocationBar().getFirstUrlBarFocusTime();
        if (firstFocusTime != 0) {
            RecordHistogram.recordCustomTimesHistogram(
                    "MobileStartup.ToolbarFirstFocusTime." + className,
                    firstFocusTime - creationTime, MIN_FOCUS_TIME_FOR_UMA_HISTOGRAM_MS,
                    MAX_FOCUS_TIME_FOR_UMA_HISTOGRAM_MS, TimeUnit.MILLISECONDS, 50);
        }
    }

    /**
     * Finish any toolbar animations.
     */
    public void finishAnimations() {
        if (isInitialized()) mToolbar.finishAnimations();
    }
}
