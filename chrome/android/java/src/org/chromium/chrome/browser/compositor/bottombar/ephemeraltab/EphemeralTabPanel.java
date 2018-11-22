// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.content.Context;
import android.graphics.RectF;
import android.view.MotionEvent;

import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.PanelPriority;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.OverlayPanelEventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.EphemeralTabSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.SceneOverlayLayer;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.resources.ResourceManager;

/**
 * The panel containing an ephemeral tab.
 * TODO(jinsukkim): Make panel height in peeked state bigger to show the progress indicator clearly.
 */
public class EphemeralTabPanel extends OverlayPanel {
    /** The compositor layer used for drawing the panel. */
    private EphemeralTabSceneLayer mSceneLayer;

    /**
     * Checks if this feature (a.k.a. "Sneak peek") for html and image is supported.
     * @return {@code true} if the feature is enabled.
     */
    public static boolean isSupported() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.EPHEMERAL_TAB)
                && !SysUtils.isLowEndDevice();
    }

    /**
     * @param context The current Android {@link Context}.
     * @param updateHost The {@link LayoutUpdateHost} used to request updates in the Layout.
     * @param panelManager The {@link OverlayPanelManager} used to control panel show/hide.
     */
    public EphemeralTabPanel(
            Context context, LayoutUpdateHost updateHost, OverlayPanelManager panelManager) {
        super(context, updateHost, panelManager);
        mSceneLayer =
                new EphemeralTabSceneLayer(mContext.getResources().getDisplayMetrics().density);
        mEventFilter = new OverlayPanelEventFilter(mContext, this) {
            @Override
            public boolean onInterceptTouchEventInternal(MotionEvent e, boolean isKeyboardShowing) {
                OverlayPanel panel = EphemeralTabPanel.this;
                if (panel.isShowing() && panel.isPeeking()
                        && panel.isCoordinateInsideBar(e.getX() * mPxToDp, e.getY() * mPxToDp)) {
                    // Events go to base panel in peeked mode to scroll base page.
                    return super.onInterceptTouchEventInternal(e, isKeyboardShowing);
                }
                if (panel.isShowing() && panel.isMaximized()) return true;
                return false;
            }
        };
    }

    @Override
    public OverlayPanelContent createNewOverlayPanelContent() {
        return new OverlayPanelContent(new OverlayContentDelegate(), new PanelProgressObserver(),
                mActivity, getBarHeight());
    }

    @Override
    protected float getMaximizedHeight() {
        // Max height does not cover the entire content screen.
        return getTabHeight() * 0.9f;
    }

    @Override
    public float getProgressBarOpacity() {
        return 1.0f;
    }

    // Scene Overlay

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(RectF viewport, RectF visibleViewport,
            LayerTitleCache layerTitleCache, ResourceManager resourceManager, float yOffset) {
        mSceneLayer.update(resourceManager, this, getBarTextViewId(), 1.0f);
        return mSceneLayer;
    }

    @Override
    public boolean updateOverlay(long time, long dt) {
        // Allow WebContents to size itself appropriately (includes browser controls height).
        updateBrowserControlsState();
        return super.updateOverlay(time, dt);
    }

    // Generic Event Handling

    @Override
    public void handleBarClick(float x, float y) {
        super.handleBarClick(x, y);
        if (isCoordinateInsideCloseButton(x)) {
            closePanel(StateChangeReason.CLOSE_BUTTON, true);
        } else {
            maximizePanel(StateChangeReason.SEARCH_BAR_TAP);
        }
    }

    // Panel base methods

    @Override
    public void destroyComponents() {
        super.destroyComponents();
        destroyEphemeralTabBarControl();
    }

    @Override
    public @PanelPriority int getPriority() {
        return PanelPriority.HIGH;
    }

    @Override
    protected boolean isSupportedState(PanelState state) {
        return state != PanelState.EXPANDED;
    }

    @Override
    protected void onClosed(@StateChangeReason int reason) {
        super.onClosed(reason);
        if (mSceneLayer != null) mSceneLayer.hideTree();
    }

    /**
     * Request opening the ephemeral tab panel when triggered from context menu.
     * @param url URL of the content to open in the panel
     * @param text Link text which will appear on the tab bar.
     */
    public void requestOpenPanel(String url, String text) {
        loadUrlInPanel(url);
        WebContents panelWebContents = getWebContents();
        if (panelWebContents != null) panelWebContents.onShow();
        getEphemeralTabBarControl().setBarText(text);
        requestPanelShow(StateChangeReason.CLICK);
    }

    @Override
    public void onLayoutChanged(float width, float height, float visibleViewportOffsetY) {
        if (width != getWidth()) destroyEphemeralTabBarControl();
        super.onLayoutChanged(width, height, visibleViewportOffsetY);
    }

    private EphemeralTabBarControl mEphemeralTabBarControl;

    /**
     * @return The Id of the Search Term View.
     */
    public int getBarTextViewId() {
        return getEphemeralTabBarControl().getViewId();
    }

    /**
     * Creates the EphemeralTabBarControl, if needed. The Views are set to INVISIBLE, because
     * they won't actually be displayed on the screen (their snapshots will be displayed instead).
     */
    protected EphemeralTabBarControl getEphemeralTabBarControl() {
        assert mContainerView != null;
        assert mResourceLoader != null;
        if (mEphemeralTabBarControl == null) {
            mEphemeralTabBarControl =
                    new EphemeralTabBarControl(this, mContext, mContainerView, mResourceLoader);
        }
        assert mEphemeralTabBarControl != null;
        return mEphemeralTabBarControl;
    }

    /**
     * Destroys the EphemeralTabBarControl.
     */
    protected void destroyEphemeralTabBarControl() {
        if (mEphemeralTabBarControl != null) {
            mEphemeralTabBarControl.destroy();
            mEphemeralTabBarControl = null;
        }
    }
}
