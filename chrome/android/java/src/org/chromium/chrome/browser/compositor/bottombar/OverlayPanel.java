// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.PanelPriority;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.ContentVideoViewEmbedder;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.common.TopControlsState;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.ResourceManager;

/**
 * Controls the Overlay Panel.
 */
public class OverlayPanel extends OverlayPanelAnimation implements ActivityStateListener,
        OverlayPanelContentFactory {

    /**
     * The extra dp added around the close button touch target.
     */
    private static final int CLOSE_BUTTON_TOUCH_SLOP_DP = 5;

    /**
     * State of the Overlay Panel.
     */
    public static enum PanelState {
        UNDEFINED,
        CLOSED,
        PEEKED,
        EXPANDED,
        MAXIMIZED;
    }

    /**
     * The reason for a change in the Overlay Panel's state.
     * TODO(mdjones): Separate generic reasons from Contextual Search reasons.
     */
    public static enum StateChangeReason {
        UNKNOWN,
        RESET,
        BACK_PRESS,
        TEXT_SELECT_TAP,
        TEXT_SELECT_LONG_PRESS,
        INVALID_SELECTION,
        CLEARED_SELECTION,
        BASE_PAGE_TAP,
        BASE_PAGE_SCROLL,
        SEARCH_BAR_TAP,
        SERP_NAVIGATION,
        TAB_PROMOTION,
        CLICK,
        SWIPE,
        FLING,
        OPTIN,
        OPTOUT,
        CLOSE_BUTTON,
        SUPPRESS,
        UNSUPPRESS,
        FULLSCREEN_ENTERED,
        FULLSCREEN_EXITED,
        INFOBAR_SHOWN,
        INFOBAR_HIDDEN,
        CONTENT_CHANGED,
        KEYBOARD_SHOWN,
        KEYBOARD_HIDDEN,
        TAB_NAVIGATION;
    }

    /** The activity this panel is in. */
    protected ChromeActivity mActivity;

    /** The initial height of the Overlay Panel. */
    private float mInitialPanelHeight;

    /** Whether a touch gesture has been detected. */
    private boolean mHasDetectedTouchGesture;

    /** That factory that creates OverlayPanelContents. */
    private OverlayPanelContentFactory mContentFactory;

    /** Container for content the panel will show. */
    private OverlayPanelContent mContent;

    /** The {@link OverlayPanelHost} used to communicate with the supported layout. */
    private OverlayPanelHost mOverlayPanelHost;

    /** OverlayPanel manager handle for notifications of opening and closing. */
    protected OverlayPanelManager mPanelManager;

    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * @param context The current Android {@link Context}.
     * @param updateHost The {@link LayoutUpdateHost} used to request updates in the Layout.
     * @param panelManager The {@link OverlayPanelManager} responsible for showing panels.
     */
    public OverlayPanel(Context context, LayoutUpdateHost updateHost,
                OverlayPanelManager panelManager) {
        super(context, updateHost);
        mContentFactory = this;

        mPanelManager = panelManager;
        mPanelManager.registerPanel(this);
    }

    /**
     * Destroy the native components associated with this panel's content.
     */
    public void destroy() {
        closePanel(StateChangeReason.UNKNOWN, false);
        ApplicationStatus.unregisterActivityStateListener(this);
    }

    /**
     * Destroy the components associated with this panel. This should be overridden by panel
     * implementations to destroy text views and other elements.
     */
    protected void destroyComponents() {
        destroyOverlayPanelContent();
    }

    @Override
    protected void onClosed(StateChangeReason reason) {
        destroyComponents();
        mPanelManager.notifyPanelClosed(this, reason);
    }

    // ============================================================================================
    // General API
    // ============================================================================================

    /**
     * @return True if the panel is in the PEEKED state.
     */
    public boolean isPeeking() {
        return doesPanelHeightMatchState(PanelState.PEEKED);
    }

    /**
     * @return Whether the Panel is in its expanded state.
     */
    public boolean isExpanded() {
        return doesPanelHeightMatchState(PanelState.EXPANDED);
    }

    @Override
    public void closePanel(StateChangeReason reason, boolean animate) {
        if (!isShowing()) return;

        super.closePanel(reason, animate);

        // If the close action is animated, the Layout will be hidden when
        // the animation is finished, so we should only hide the Layout
        // here when not animating.
        if (!animate && mOverlayPanelHost != null) {
            mOverlayPanelHost.hideLayout(true);
        }
    }

    /**
     * Request that this panel be shown.
     * @param reason The reason the panel is being shown.
     */
    public void requestPanelShow(StateChangeReason reason) {
        if (isShowing()) return;

        if (mPanelManager != null) {
            mPanelManager.requestPanelShow(this, reason);
        }
    }

    @Override
    public void peekPanel(StateChangeReason reason) {
        // TODO(mdjones): This is making a protected API public and should be removed. Animation
        // should only be controlled by the OverlayPanelManager.
        super.peekPanel(reason);
    }

    /**
     * @param url The URL that the panel should load.
     */
    public void loadUrlInPanel(String url) {
        getOverlayPanelContent().loadUrl(url, true);
    }

    /**
     * @param url The URL that the panel should load.
     * @param shouldLoadImmediately If the URL should be loaded immediately when this method is
     *                              called.
     */
    public void loadUrlInPanel(String url, boolean shouldLoadImmediately) {
        getOverlayPanelContent().loadUrl(url, shouldLoadImmediately);
    }

    /**
     * @return True if a URL has been loaded in the panel's current ContentViewCore.
     */
    public boolean isProcessingPendingNavigation() {
        return mContent != null && mContent.isProcessingPendingNavigation();
    }

    /**
     * @param activity The ChromeActivity associated with the panel.
     */
    public void setChromeActivity(ChromeActivity activity) {
        mActivity = activity;
        if (mActivity != null) {
            ApplicationStatus.registerStateListenerForActivity(this, mActivity);
        }
    }

    /**
     * Notify the panel's content that it has been touched.
     * @param x The X position of the touch in dp.
     */
    public void notifyBarTouched(float x) {
        if (!isCoordinateInsideCloseButton(x)) {
            getOverlayPanelContent().notifyBarTouched();
        }
    }

    /**
     * Acknowledges that there was a touch in the search content view, though no immediate action
     * needs to be taken. This should be overridden by child classes.
     * TODO(mdjones): Get a better name for this.
     */
    public void onTouchSearchContentViewAck() {
    }

    /**
     * Get a panel's display priority. This has a default to MEDIUM and should be overridden by
     * child classes.
     * @return The panel's display priority.
     */
    public PanelPriority getPriority() {
        return PanelPriority.MEDIUM;
    }

    /**
     * @return True if a panel can be suppressed. This should be overridden by each panel.
     */
    public boolean canBeSuppressed() {
        return false;
    }

    @Override
    protected float getTopControlsOffsetDp() {
        if (mActivity == null || mActivity.getFullscreenManager() == null) return 0.0f;
        return -mActivity.getFullscreenManager().getControlOffset() * mPxToDp;
    }

    // ============================================================================================
    // ActivityStateListener
    // ============================================================================================

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.RESUMED || newState == ActivityState.STOPPED
                || newState == ActivityState.DESTROYED) {
            closePanel(StateChangeReason.UNKNOWN, false);
        }
    }

    // ============================================================================================
    // Content
    // ============================================================================================

    /**
     * @return True if the panel's content view is showing.
     */
    public boolean isContentShowing() {
        return mContent != null && mContent.isContentShowing();
    }

    /**
     * @return The ContentViewCore that this panel currently holds.
     */
    public ContentViewCore getContentViewCore() {
        // Expose OverlayPanelContent method.
        return mContent != null ? mContent.getContentViewCore() : null;
    }

    /**
     * Create a new OverlayPanelContent object. This can be overridden for tests.
     * @return A new OverlayPanelContent object.
     */
    @Override
    public OverlayPanelContent createNewOverlayPanelContent() {
        return new OverlayPanelContent(new OverlayContentDelegate(),
                new OverlayContentProgressObserver(), mActivity);
    }

    /**
     * Add any other objects that depend on the OverlayPanelContent having already been created.
     */
    private OverlayPanelContent createNewOverlayPanelContentInternal() {
        OverlayPanelContent content = mContentFactory.createNewOverlayPanelContent();

        content.setContentViewClient(new ContentViewClient() {
            @Override
            public int getDesiredWidthMeasureSpec() {
                if (isFullWidthSizePanel()) {
                    return super.getDesiredWidthMeasureSpec();
                } else {
                    return MeasureSpec.makeMeasureSpec(
                            getContentViewWidthPx(),
                            MeasureSpec.EXACTLY);
                }
            }

            @Override
            public int getDesiredHeightMeasureSpec() {
                if (isFullWidthSizePanel()) {
                    return super.getDesiredHeightMeasureSpec();
                } else {
                    return MeasureSpec.makeMeasureSpec(
                            getContentViewHeightPx(),
                            MeasureSpec.EXACTLY);
                }
            }

            @Override
            public ContentVideoViewEmbedder getContentVideoViewEmbedder() {
                // TODO(mdjones): Possibly enable fullscreen video in overlay panels rather than
                // passing an empty implementation.
                return new ContentVideoViewEmbedder() {
                    @Override
                    public void enterFullscreenVideo(View view) {}

                    @Override
                    public void exitFullscreenVideo() {}

                    @Override
                    public View getVideoLoadingProgressView() {
                        return null;
                    }

                    @Override
                    public void setSystemUiVisibility(boolean enterFullscreen) {}
                };
            }
        });

        return content;
    }

    /**
     * @return A new OverlayPanelContent if the instance was null or the existing one.
     */
    protected OverlayPanelContent getOverlayPanelContent() {
        // Only create the content when necessary
        if (mContent == null) {
            mContent = createNewOverlayPanelContentInternal();
        }
        return mContent;
    }

    /**
     * Destroy the native components of the OverlayPanelContent.
     */
    protected void destroyOverlayPanelContent() {
        // It is possible that an OverlayPanelContent was never created for this panel.
        if (mContent != null) {
            mContent.destroy();
            mContent = null;
        }
    }

    /**
     * Updates the top controls state for the base tab.  As these values are set at the renderer
     * level, there is potential for this impacting other tabs that might share the same
     * process. See {@link Tab#updateTopControlsState(int current, boolean animate)}
     * @param current The desired current state for the controls.  Pass
     *                {@link TopControlsState#BOTH} to preserve the current position.
     * @param animate Whether the controls should animate to the specified ending condition or
     *                should jump immediately.
     */
    public void updateTopControlsState(int current, boolean animate) {
        Tab currentTab = mActivity.getActivityTab();
        if (currentTab != null) {
            currentTab.updateTopControlsState(current, animate);
        }
    }

    /**
     * Sets the top control state based on the internals of the panel.
     */
    public void updateTopControlsState() {
        if (mContent == null) return;

        if (isFullWidthSizePanel()) {
            // Consider the ContentView height to be fullscreen, and inform the system that
            // the Toolbar is always visible (from the Compositor's perspective), even though
            // the Toolbar and Base Page might be offset outside the screen. This means the
            // renderer will consider the ContentView height to be the fullscreen height
            // minus the Toolbar height.
            //
            // This is necessary to fix the bugs: crbug.com/510205 and crbug.com/510206
            mContent.updateTopControlsState(false, true, false);
        } else {
            mContent.updateTopControlsState(true, false, false);
        }
    }

    /**
     * Remove the last entry from history provided it is in a given time frame.
     * @param historyUrl The URL to remove.
     * @param urlTimeMs The time that the URL was visited.
     */
    public void removeLastHistoryEntry(String historyUrl, long urlTimeMs) {
        if (mContent == null) return;
        // Expose OverlayPanelContent method.
        mContent.removeLastHistoryEntry(historyUrl, urlTimeMs);
    }

    /**
     * @return The vertical scroll position of the content.
     */
    public float getContentVerticalScroll() {
        return mContent != null ? mContent.getContentVerticalScroll() : 0.0f;
    }

    // ============================================================================================
    // Animation Handling
    // ============================================================================================

    @Override
    protected void onAnimationFinished() {
        super.onAnimationFinished();

        if (shouldHideOverlayPanelLayout()) {
            if (mOverlayPanelHost != null) {
                mOverlayPanelHost.hideLayout(false);
            }
        }
    }

    /**
     * Whether the Overlay Panel Layout should be hidden.
     *
     * @return Whether the Overlay Panel Layout should be hidden.
     */
    private boolean shouldHideOverlayPanelLayout() {
        final PanelState state = getPanelState();
        return (state == PanelState.PEEKED || state == PanelState.CLOSED)
                && getHeight() == getPanelHeightFromState(state);
    }

    // ============================================================================================
    // ContextualSearchPanelBase methods.
    // ============================================================================================

    @Override
    protected int getControlContainerHeightResource() {
        // TODO(mdjones): Investigate passing this in to the constructor instead.
        assert mActivity != null;
        return mActivity.getControlContainerHeightResource();
    }

    // ============================================================================================
    // Layout Integration
    // ============================================================================================

    /**
     * Sets the {@OverlayPanelHost} used to communicate with the supported layout.
     * @param host The {@OverlayPanelHost}.
     */
    public void setHost(OverlayPanelHost host) {
        mOverlayPanelHost = host;
    }

    /**
     * @return The scene layer used to draw this panel.
     */
    public SceneLayer getSceneLayer() {
        return null;
    }

    /**
     * Update this panel's scene layer. This should be implemented by each panel type.
     * @param resourceManager Used to access static resources.
     */
    public void updateSceneLayer(ResourceManager resourceManager) {
    }

    /**
     * Determine if using a second layout for showing the overlay panel is possible. This should
     * be overridden by each panel and returns true by default.
     * @return True if the layout is supported.
     * TODO(mdjones): Rename to supportsOverlayPanelLayout once the corresponding class is renamed.
     */
    public boolean supportsContextualSearchLayout() {
        return true;
    }

    /**
     * Handles the device orientation change.
     */
    public void onOrientationChanged() {
        if (isNarrowSizePanelSupported()) {
            // TODO(pedrosimonetti): We cannot preserve the panel when rotating from/to a
            // narrow version of the Panel because the Content is resized before the Panel
            // gets a notification of the resize.
            closePanel(StateChangeReason.UNKNOWN, false);
        } else {
            updatePanelForOrientationChange();
        }
    }

    /**
     * Updates the Panel so it preserves its state when the orientation changes.
     */
    protected void updatePanelForOrientationChange() {
        // NOTE(pedrosimonetti): We cannot tell where the selection will be after the
        // orientation change, so we are setting the selection position to zero, which
        // means the base page will be positioned in its original state and we won't
        // try to keep the selection in view.
        updateBasePageSelectionYPx(0.f);
        resizePanelToState(getPanelState(), StateChangeReason.UNKNOWN);
    }

    // ============================================================================================
    // Generic Event Handling
    // ============================================================================================

    /**
     * Handles the beginning of the swipe gesture.
     */
    public void handleSwipeStart() {
        if (animationIsRunning()) {
            cancelHeightAnimation();
        }

        mHasDetectedTouchGesture = false;
        mInitialPanelHeight = getHeight();
    }

    /**
     * Handles the movement of the swipe gesture.
     *
     * @param ty The movement's total displacement in dps.
     */
    public void handleSwipeMove(float ty) {
        if (mContent != null && ty > 0 && getPanelState() == PanelState.MAXIMIZED) {
            // Resets the Content View scroll position when swiping the Panel down
            // after being maximized.
            mContent.resetContentViewScroll();
        }

        // Negative ty value means an upward movement so subtracting ty means expanding the panel.
        setClampedPanelHeight(mInitialPanelHeight - ty);
        requestUpdate();
    }

    /**
     * Handles the end of the swipe gesture.
     */
    public void handleSwipeEnd() {
        // This method will be called after handleFling() and handleClick()
        // methods because we also need to track down the onUpOrCancel()
        // action from the Layout. Therefore the animation to the nearest
        // PanelState should only happen when no other gesture has been
        // detected.
        if (!mHasDetectedTouchGesture) {
            mHasDetectedTouchGesture = true;
            animateToNearestState();
        }
    }

    /**
     * Handles the fling gesture.
     *
     * @param velocity The velocity of the gesture in dps per second.
     */
    public void handleFling(float velocity) {
        mHasDetectedTouchGesture = true;
        animateToProjectedState(velocity);
    }

    /**
     * @param x The x coordinate in dp.
     * @return Whether the given |x| coordinate is inside the close button.
     */
    protected boolean isCoordinateInsideCloseButton(float x) {
        if (LocalizationUtils.isLayoutRtl()) {
            return x <= (getCloseIconX() + getCloseIconDimension() + CLOSE_BUTTON_TOUCH_SLOP_DP);
        } else {
            return x >= (getCloseIconX() - CLOSE_BUTTON_TOUCH_SLOP_DP);
        }
    }

    /**
     * Handles the click gesture.
     *
     * @param time The timestamp of the gesture.
     * @param x The x coordinate of the gesture.
     * @param y The y coordinate of the gesture.
     */
    public void handleClick(long time, float x, float y) {
        mHasDetectedTouchGesture = true;
        if (isCoordinateInsideBasePage(x, y)) {
            closePanel(StateChangeReason.BASE_PAGE_TAP, true);
        } else if (isCoordinateInsideBar(x, y) && !onInterceptBarClick()) {
            handleBarClick(time, x, y);
        }
    }

    /**
     * Handles the click gesture specifically on the bar.
     *
     * @param time The timestamp of the gesture.
     * @param x The x coordinate of the gesture.
     * @param y The y coordinate of the gesture.
     */
    protected void handleBarClick(long time, float x, float y) {
        if (isPeeking()) {
            expandPanel(StateChangeReason.SEARCH_BAR_TAP);
        }
    }

    /**
     * Allows the click on the bar to be intercepted.
     * @return True if the click on the bar was intercepted by this function.
     */
    protected boolean onInterceptBarClick() {
        return false;
    }

    /**
     * If the panel is intercepting the initial bar swipe event. This should be overridden per
     * panel.
     * @return True if the panel intercepted the initial bar swipe.
     */
    public boolean onInterceptBarSwipe() {
        return false;
    }

    // ============================================================================================
    // Gesture Event helpers
    // ============================================================================================

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the bar area of the overlay.
     */
    public boolean isCoordinateInsideBar(float x, float y) {
        return isCoordinateInsideOverlayPanel(x, y)
                && y >= getOffsetY() && y <= (getOffsetY() + getBarContainerHeight());
    }

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the Overlay Content View area.
     */
    public boolean isCoordinateInsideContent(float x, float y) {
        return isCoordinateInsideOverlayPanel(x, y)
                && y > getContentY();
    }

    /**
     * @return The horizontal offset of the Overlay Content View in dp.
     */
    public float getContentX() {
        return getOffsetX();
    }

    /**
     * @return The vertical offset of the Overlay Content View in dp.
     */
    public float getContentY() {
        return getOffsetY() + getBarContainerHeight() + getPromoHeight();
    }

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the Overlay Panel area.
     */
    private boolean isCoordinateInsideOverlayPanel(float x, float y) {
        return y >= getOffsetY() && y <= (getOffsetY() + getHeight())
                &&  x >= getOffsetX() && x <= (getOffsetX() + getWidth());
    }

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the Base Page area.
     */
    private boolean isCoordinateInsideBasePage(float x, float y) {
        return !isCoordinateInsideOverlayPanel(x, y);
    }

    @VisibleForTesting
    public void setOverlayPanelContentFactory(OverlayPanelContentFactory factory) {
        mContentFactory = factory;
    }
}
