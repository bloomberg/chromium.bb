// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.ToolbarSwipeLayout;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.MenuButton;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsViewBinder.ViewHolder;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.ResourceManager;

/**
 * The root coordinator for the bottom controls component. This component is intended for use with
 * bottom UI that re-sizes the web contents, scrolls off-screen, and hides when the keyboard is
 * shown. This class has two primary components, an Android view and a composited texture that draws
 * when the controls are being scrolled off-screen. The Android version does not draw unless the
 * controls offset is 0.
 */
public class BottomControlsCoordinator {
    /** The mediator that handles events from outside the bottom controls. */
    private final BottomControlsMediator mMediator;

    /** The coordinator for the split toolbar's bottom toolbar component. */
    private final BottomToolbarCoordinator mBottomToolbarCoordinator;

    /**
     * Build the coordinator that manages the bottom controls.
     * @param fullscreenManager A {@link ChromeFullscreenManager} to update the bottom controls
     *                          height for the renderer.
     * @param stub The bottom controls {@link ViewStub} to inflate.
     * @param tabProvider The {@link ActivityTabProvider} used in the bottom toolbar.
     * @param homeButtonListener The {@link OnClickListener} for the bottom toolbar's home button.
     * @param searchAcceleratorListener The {@link OnClickListener} for the bottom toolbar's
     *                                  search accelerator.
     * @param shareButtonListener The {@link OnClickListener} for the bottom toolbar's share button.
     */
    public BottomControlsCoordinator(ChromeFullscreenManager fullscreenManager, ViewStub stub,
            ActivityTabProvider tabProvider, OnClickListener homeButtonListener,
            OnClickListener searchAcceleratorListener, OnClickListener shareButtonListener) {
        final ScrollingBottomViewResourceFrameLayout root =
                (ScrollingBottomViewResourceFrameLayout) stub.inflate();

        PropertyModel model = new PropertyModel(BottomControlsProperties.ALL_KEYS);

        PropertyModelChangeProcessor.create(
                model, new ViewHolder(root), BottomControlsViewBinder::bind);

        mMediator = new BottomControlsMediator(model, fullscreenManager,
                root.getResources().getDimensionPixelOffset(R.dimen.bottom_toolbar_height));

        mBottomToolbarCoordinator = new BottomToolbarCoordinator(
                root.findViewById(R.id.bottom_toolbar_stub), tabProvider, homeButtonListener,
                searchAcceleratorListener, shareButtonListener);
    }

    /**
     * Initialize the bottom controls with the components that had native initialization
     * dependencies.
     * <p>
     * Calling this must occur after the native library have completely loaded.
     * @param resourceManager A {@link ResourceManager} for loading textures into the compositor.
     * @param layoutManager A {@link LayoutManager} to attach overlays to.
     * @param tabSwitcherListener An {@link OnClickListener} that is triggered when the
     *                            bottom toolbar's tab switcher button is clicked.
     * @param newTabClickListener An {@link OnClickListener} that is triggered when the
     *                            bottom toolbar's new tab button is clicked.
     * @param menuButtonHelper An {@link AppMenuButtonHelper} that is triggered when the
     *                         bottom toolbar's menu button is clicked.
     * @param overviewModeBehavior The overview mode manager.
     * @param windowAndroid A {@link WindowAndroid} for watching keyboard visibility events.
     * @param tabCountProvider Updates the tab count number in the tab switcher button and in the
     *                         incognito toggle tab layout.
     * @param incognitoStateProvider Notifies components when incognito mode is entered or exited.
     * @param topToolbarRoot The root {@link ViewGroup} of the top toolbar.
     */
    public void initializeWithNative(ResourceManager resourceManager, LayoutManager layoutManager,
            OnClickListener tabSwitcherListener, OnClickListener newTabClickListener,
            OnClickListener closeTabsClickListener, AppMenuButtonHelper menuButtonHelper,
            OverviewModeBehavior overviewModeBehavior, WindowAndroid windowAndroid,
            TabCountProvider tabCountProvider, IncognitoStateProvider incognitoStateProvider,
            ViewGroup topToolbarRoot) {
        mMediator.setLayoutManager(layoutManager);
        mMediator.setResourceManager(resourceManager);
        mMediator.setToolbarSwipeHandler(layoutManager.getToolbarSwipeHandler());
        mMediator.setWindowAndroid(windowAndroid);

        mBottomToolbarCoordinator.initializeWithNative(tabSwitcherListener, newTabClickListener,
                closeTabsClickListener, menuButtonHelper, overviewModeBehavior, tabCountProvider,
                incognitoStateProvider, topToolbarRoot);
    }

    /**
     * @param isVisible Whether the bottom control is visible.
     */
    public void setBottomControlsVisible(boolean isVisible) {
        mMediator.setBottomControlsVisible(isVisible);
        mBottomToolbarCoordinator.setBottomToolbarVisible(isVisible);
    }

    /**
     * Show the update badge over the bottom toolbar's app menu.
     */
    public void showAppMenuUpdateBadge() {
        mBottomToolbarCoordinator.showAppMenuUpdateBadge();
    }

    /**
     * Remove the update badge.
     */
    public void removeAppMenuUpdateBadge() {
        mBottomToolbarCoordinator.removeAppMenuUpdateBadge();
    }

    /**
     * @return Whether the update badge is showing.
     */
    public boolean isShowingAppMenuUpdateBadge() {
        return mBottomToolbarCoordinator.isShowingAppMenuUpdateBadge();
    }

    /**
     * @return The wrapper for the browsing mode toolbar's app menu button.
     */
    public MenuButton getMenuButtonWrapper() {
        return mBottomToolbarCoordinator.getMenuButtonWrapper();
    }

    /**
     * @param layout The {@link ToolbarSwipeLayout} that the bottom controls will hook into. This
     *               allows the bottom controls to provide the layout with scene layers with the
     *               bottom controls' texture.
     */
    public void setToolbarSwipeLayout(ToolbarSwipeLayout layout) {
        mMediator.setToolbarSwipeLayout(layout);
    }

    /**
     * Clean up any state when the bottom controls component is destroyed.
     */
    public void destroy() {
        mBottomToolbarCoordinator.destroy();
        mMediator.destroy();
    }
}
