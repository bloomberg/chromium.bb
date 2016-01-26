// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.view.View;

import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.layouts.ContextualSearchSupportedLayout;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeEventFilter.ScrollDirection;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.TabListSceneLayer;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.common.TopControlsState;
import org.chromium.ui.resources.ResourceManager;

/**
 * A {@link Layout} that shows a Contextual Search overlay at the bottom.
 */
public class ContextualSearchLayout extends ContextualSearchSupportedLayout {
    /**
     * The initial Y position of the touch event.
     */
    private float mInitialPanelTouchY;

    /**
     * The base Tab.
     */
    private LayoutTab mBaseTab;

    private final TabListSceneLayer mTabListSceneLayer;

    /**
     * @param context The current Android context.
     * @param updateHost The {@link LayoutUpdateHost} view for this layout.
     * @param renderHost The {@link LayoutRenderHost} view for this layout.
     * @param eventFilter The {@link EventFilter} that is needed for this view.
     * @param panelManager The {@link OverlayPanelManager} for getting the active panel.
     */
    public ContextualSearchLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, EventFilter eventFilter,
            OverlayPanelManager panelManager) {
        super(context, updateHost, renderHost, eventFilter, panelManager);
        mTabListSceneLayer = new TabListSceneLayer() {
            @Override
            protected int getTabListBackgroundColor(Context context) {
                OverlayPanel panel = mPanelManager.getActivePanel();
                // If the panel is null (which in theory should never happen in this case since
                // this layout is only present when the Panel is open), we will assume the
                // background color to be white (assuming most pages have a white background).
                if (panel == null) {
                    return Color.WHITE;
                } else {
                    return panel.getBasePageBackgroundColor();
                }
            }
        };
    }

    /**
     * Handles the resizing of the viewport.
     * @param width  The new width in dp.
     * @param height The new height in dp.
     */
    @Override
    protected void onSizeChanged(float width, float height) {
        if (mBaseTab != null) {
            OverlayPanel panel = mPanelManager.getActivePanel();
            if (panel != null) {
                mBaseTab.setMaxContentWidth(width);
                mBaseTab.setMaxContentHeight(panel.getTabHeight());
                mBaseTab.setContentSize(width, panel.getTabHeight());
            }
        }
    }

    @Override
    public View getViewForInteraction() {
        OverlayPanel panel = mPanelManager.getActivePanel();
        if (panel != null) {
            ContentViewCore content = panel.getContentViewCore();
            if (content != null) return content.getContainerView();
        }

        return super.getViewForInteraction();
    }

    @Override
    public int getSizingFlags() {
        return SizingFlags.HELPER_SUPPORTS_FULLSCREEN;
    }

    @Override
    public float getTopControlsOffset(float currentOffsetDp) {
        return MathUtils.clamp(mBaseTab.getY(), -mPanelManager.getActivePanel().getToolbarHeight(),
                Math.min(currentOffsetDp, 0f));
    }

    @Override
    protected void updateLayout(long time, long dt) {
        super.updateLayout(time, dt);
        if (mBaseTab == null) return;

        OverlayPanel panel = mPanelManager.getActivePanel();
        if (panel != null) {
            mBaseTab.setY(panel.getBasePageY());
            mBaseTab.setBrightness(panel.getBasePageBrightness());
        }

        boolean needUpdate = mBaseTab.updateSnap(dt);
        if (needUpdate) requestUpdate();
    }

    @Override
    public boolean handlesTabCreating() {
        // Prevents the new Tab animation from happening when promoting to a new Tab.
        startHiding(mBaseTab.getId(), false);
        doneHiding();
        // Updates TopControls' State so the Toolbar becomes visible.
        // TODO(pedrosimonetti): The transition when promoting to a new tab is only smooth
        // if the SearchContentView's vertical scroll position is zero. Otherwise the
        // ContentView will appear to jump in the screen. Coordinate with @dtrainor to solve
        // this problem.
        mPanelManager.getActivePanel().updateTopControlsState(TopControlsState.BOTH, false);
        return true;
    }

    @Override
    public void show(long time, boolean animate) {
        // TODO(changwan): use SceneOverlayTree's setContentTree() instead once we refactor
        // ContextualSearchSupportedLayout into LayoutHelper.
        mTabListSceneLayer.setContentTree(super.getSceneLayer());

        super.show(time, animate);

        resetLayout();
        createBaseLayoutTab(mBaseTab);
    }

    /**
     * Resets the Layout.
     */
    private void resetLayout() {
        mLayoutTabs = null;
        mBaseTab = null;
    }

    /**
     * Creates the Base Page's LayoutTab to be presented in the screen.
     *
     * @param layoutTab The {@link Layout} instance.
     */
    private void createBaseLayoutTab(LayoutTab layoutTab) {
        if (mTabModelSelector == null) return;

        int baseTabId = mTabModelSelector.getCurrentTabId();
        if (baseTabId == Tab.INVALID_TAB_ID) return;

        mBaseTab = createLayoutTab(
                baseTabId, mTabModelSelector.isIncognitoSelected(), NO_CLOSE_BUTTON, NO_TITLE);

        assert mBaseTab != null;
        mBaseTab.setScale(1.f);
        mBaseTab.setBorderScale(1.f);
        mBaseTab.setBorderAlpha(0.f);

        mLayoutTabs = new LayoutTab[] {mBaseTab};
    }

    // ============================================================================================
    // Panel Host
    // ============================================================================================

    @Override
    protected void hideContextualSearch(boolean immediately) {
        if (isActive()) {
            startHiding(Tab.INVALID_TAB_ID, false);
            if (immediately) doneHiding();
        }
    }

    // ============================================================================================
    // cc::Layer Events
    // ============================================================================================

    @Override
    public void onDown(long time, float x, float y) {
        mInitialPanelTouchY = y;
        mPanelManager.getActivePanel().handleSwipeStart();
    }

    @Override
    public void drag(long time, float x, float y, float deltaX, float deltaY) {
        final float ty = y - mInitialPanelTouchY;

        mPanelManager.getActivePanel().handleSwipeMove(ty);
    }

    @Override
    public void onUpOrCancel(long time) {
        mPanelManager.getActivePanel().handleSwipeEnd();
    }

    @Override
    public void fling(long time, float x, float y, float velocityX, float velocityY) {
        mPanelManager.getActivePanel().handleFling(velocityY);
    }

    @Override
    public void click(long time, float x, float y) {
        mPanelManager.getActivePanel().handleClick(time, x, y);
    }

    // ============================================================================================
    // Android View Events
    // ============================================================================================

    @Override
    public void swipeStarted(long time, ScrollDirection direction, float x, float y) {
        mPanelManager.getActivePanel().handleSwipeStart();
    }

    @Override
    public void swipeUpdated(long time, float x, float y, float dx, float dy, float tx, float ty) {
        mPanelManager.getActivePanel().handleSwipeMove(ty);
    }

    @Override
    public void swipeFlingOccurred(
            long time, float x, float y, float tx, float ty, float vx, float vy) {
        mPanelManager.getActivePanel().handleFling(vy);
    }

    @Override
    public void swipeFinished(long time) {
        mPanelManager.getActivePanel().handleSwipeEnd();
    }

    @Override
    public void swipeCancelled(long time) {
        swipeFinished(time);
    }

    @Override
    public boolean onBackPressed() {
        mPanelManager.getActivePanel().closePanel(StateChangeReason.BACK_PRESS, true);
        return true;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mTabListSceneLayer;
    }

    @Override
    protected void updateSceneLayer(Rect viewport, Rect contentViewport,
            LayerTitleCache layerTitleCache, TabContentManager tabContentManager,
            ResourceManager resourceManager, ChromeFullscreenManager fullscreenManager) {
        super.updateSceneLayer(viewport, contentViewport, layerTitleCache, tabContentManager,
                resourceManager, fullscreenManager);
        assert mTabListSceneLayer != null;
        mTabListSceneLayer.pushLayers(getContext(), viewport, contentViewport, this,
                layerTitleCache, tabContentManager, resourceManager);
    }

    @Override
    public boolean forceHideTopControlsAndroidView() {
        return true;
    }

    @Override
    public float getToolbarBrightness() {
        return mPanelManager.getActivePanel().getBasePageBrightness();
    }

    @Override
    public boolean isTabStripEventFilterEnabled() {
        return false;
    }
}
