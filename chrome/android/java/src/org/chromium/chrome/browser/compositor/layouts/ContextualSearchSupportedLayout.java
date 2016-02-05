// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelHost;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.ui.resources.ResourceManager;

import java.util.List;

/**
 * A {@link Layout} that can show a Contextual Search overlay that shows at the
 * bottom and can be swiped upwards.
 * TODO(mdjones): Rename this class to OverlayPanelSupportedLayout.
 */
public abstract class ContextualSearchSupportedLayout extends Layout {
    /**
     * The {@link OverlayPanelHost} that allows the {@link OverlayPanel} to
     * communicate back to the Layout.
     */
    protected final OverlayPanelHost mOverlayPanelHost;

    /**
     * The {@link OverlayPanelManager} used to get the active panel.
     */
    protected final OverlayPanelManager mPanelManager;

    /**
     * @param context The current Android context.
     * @param updateHost The {@link LayoutUpdateHost} view for this layout.
     * @param renderHost The {@link LayoutRenderHost} view for this layout.
     * @param eventFilter The {@link EventFilter} that is needed for this view.
     * @param panelManager The {@link OverlayPanelManager} responsible for showing panels.
     */
    public ContextualSearchSupportedLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, EventFilter eventFilter,
            OverlayPanelManager panelManager) {
        super(context, updateHost, renderHost, eventFilter);

        mOverlayPanelHost = new OverlayPanelHost() {
            @Override
            public void hideLayout(boolean immediately) {
                ContextualSearchSupportedLayout.this.hideContextualSearch(immediately);
            }
        };

        mPanelManager = panelManager;
    }

    @Override
    public void attachViews(ViewGroup container) {
        mPanelManager.setContainerView(container);
    }

    @Override
    public void getAllViews(List<View> views) {
        // TODO(dtrainor): If we move ContextualSearch to an overlay, pull the views from there
        // instead in Layout.java.
        OverlayPanel panel = mPanelManager.getActivePanel();
        if (panel != null) {
            ContentViewCore content = panel.getContentViewCore();
            if (content != null) views.add(content.getContainerView());
        }
        super.getAllViews(views);
    }

    @Override
    public void getAllContentViewCores(List<ContentViewCore> contents) {
        // TODO(dtrainor): If we move ContextualSearch to an overlay, pull the content from there
        // instead in Layout.java.
        OverlayPanel panel = mPanelManager.getActivePanel();
        if (panel != null) {
            ContentViewCore content = panel.getContentViewCore();
            if (content != null) contents.add(content);
        }
        super.getAllContentViewCores(contents);
    }

    @Override
    public void show(long time, boolean animate) {
        mPanelManager.setPanelHost(mOverlayPanelHost);
        super.show(time, animate);
    }

    /**
     * Hides the Contextual Search Supported Layout.
     * @param immediately Whether it should be hidden immediately.
     */
    protected void hideContextualSearch(boolean immediately) {
        // NOTE(pedrosimonetti): To be implemented by a supported Layout.
    }

    @Override
    protected void notifySizeChanged(float width, float height, int orientation) {
        super.notifySizeChanged(width, height, orientation);
        mPanelManager.onSizeChanged(width, height);
        onSizeChanged(width, height);
    }

    /**
     * Handles the resizing of the view.
     * @param width  The new width in dp.
     * @param height The new height in dp.
     */
    protected void onSizeChanged(float width, float height) {
        // NOTE(pedrosimonetti): To be implemented by a supported Layout.
    }

    @Override
    protected boolean onUpdateAnimation(long time, boolean jumpToEnd) {
        boolean parentAnimating = super.onUpdateAnimation(time, jumpToEnd);
        OverlayPanel panel = mPanelManager.getActivePanel();
        boolean panelAnimating = panel != null && panel.onUpdateAnimation(time, jumpToEnd);
        return panelAnimating || parentAnimating;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        OverlayPanel panel = mPanelManager.getActivePanel();
        if (panel == null) return null;
        return panel.getSceneLayer();
    }

    @Override
    protected void updateSceneLayer(Rect viewport, Rect contentViewport,
            LayerTitleCache layerTitleCache, TabContentManager tabContentManager,
            ResourceManager resourceManager, ChromeFullscreenManager fullscreenManager) {
        super.updateSceneLayer(viewport, contentViewport, layerTitleCache, tabContentManager,
                resourceManager, fullscreenManager);
        OverlayPanel panel = mPanelManager.getActivePanel();
        if (panel == null || !panel.isShowing()) return;

        panel.updateSceneLayer(resourceManager);
    }
}
