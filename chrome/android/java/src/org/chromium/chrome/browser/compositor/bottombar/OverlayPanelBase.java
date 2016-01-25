// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import android.content.Context;
import android.graphics.Color;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchOptOutPromo;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchOptOutPromo.ContextualSearchPromoHost;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.privacy.ContextualSearchPreferenceFragment;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Base abstract class for the Overlay Panel.
 * TODO(mdjones): Remove contextual search dependencies from this class.
 */
abstract class OverlayPanelBase implements ContextualSearchPromoHost {
    /**
     * The side padding of Bar icons in dps.
     */
    private static final float BAR_ICON_SIDE_PADDING_DP = 12.f;

    /**
     * The height of the Bar's border in dps.
     */
    private static final float BAR_BORDER_HEIGHT_DP = 1.f;

    /**
     * The height of the expanded Overlay Panel relative to the height of the screen.
     */
    private static final float EXPANDED_PANEL_HEIGHT_PERCENTAGE = .7f;

    /**
     * The width of the small version of the Overlay Panel in dps.
     */
    private static final float SMALL_PANEL_WIDTH_DP = 600.f;

    /**
     * The minimum width a screen should have in order to trigger the small version of the Panel.
     */
    private static final float SMALL_PANEL_WIDTH_THRESHOLD_DP = 680.f;

    /**
     * The brightness of the base page when the Panel is peeking.
     */
    private static final float BASE_PAGE_BRIGHTNESS_STATE_PEEKED = 1.f;

    /**
     * The brightness of the base page when the Panel is expanded.
     */
    private static final float BASE_PAGE_BRIGHTNESS_STATE_EXPANDED = .7f;

    /**
     * The brightness of the base page when the Panel is maximized. This value matches the alert
     * dialog brightness filter.
     */
    private static final float BASE_PAGE_BRIGHTNESS_STATE_MAXIMIZED = .4f;

    /**
     * The opacity of the arrow icon when the Panel is peeking.
     */
    private static final float ARROW_ICON_OPACITY_STATE_PEEKED = 1.f;

    /**
     * The opacity of the arrow icon when the Panel is expanded.
     */
    private static final float ARROW_ICON_OPACITY_STATE_EXPANDED = 0.f;

    /**
     * The opacity of the arrow icon when the Panel is maximized.
     */
    private static final float ARROW_ICON_OPACITY_STATE_MAXIMIZED = 0.f;

    /**
     * The rotation of the arrow icon.
     */
    private static final float ARROW_ICON_ROTATION = -90.f;

    /**
     * The opacity of the close icon when the Panel is peeking.
     */
    private static final float CLOSE_ICON_OPACITY_STATE_PEEKED = 0.f;

    /**
     * The opacity of the close icon when the Panel is expanded.
     */
    private static final float CLOSE_ICON_OPACITY_STATE_EXPANDED = 1.f;

    /**
     * The opacity of the close icon when the Panel is maximized.
     */
    private static final float CLOSE_ICON_OPACITY_STATE_MAXIMIZED = 1.f;

    /**
     * The id of the close icon drawable.
     */
    public static final int CLOSE_ICON_DRAWABLE_ID = R.drawable.btn_close;

    /**
     * The height of the Progress Bar in dps.
     */
    private static final float PROGRESS_BAR_HEIGHT_DP = 2.f;

    /**
     * The distance from the Progress Bar must be away from the bottom of the
     * screen in order to be completely visible. The closer the Progress Bar
     * gets to the bottom of the screen, the lower its opacity will be. When the
     * Progress Bar is at the very bottom of the screen (when the Overlay Panel
     * is peeking) it will be completely invisible.
     */
    private static final float PROGRESS_BAR_VISIBILITY_THRESHOLD_DP = 10.f;

    /**
     * The height of the Toolbar in dps.
     */
    private float mToolbarHeight;

    /**
     * The height of the Bar when the Panel is peeking, in dps.
     */
    private float mBarHeightPeeking;

    /**
     * The height of the Bar when the Panel is expanded, in dps.
     */
    private float mBarHeightExpanded;

    /**
     * The height of the Bar when the Panel is maximized, in dps.
     */
    private float mBarHeightMaximized;

    /**
     * Ratio of dps per pixel.
     */
    protected float mPxToDp;

    /**
     * The approximate Y coordinate of the selection in pixels.
     */
    private float mBasePageSelectionYPx = -1.f;

    /**
     * The Y coordinate to apply to the Base Page in order to keep the selection
     * in view when the Overlay Panel is in its EXPANDED state.
     */
    private float mBasePageTargetY = 0.f;

    /**
     * Whether the Panel is showing.
     */
    private boolean mIsShowing;

    /**
     * The current context.
     */
    protected final Context mContext;

    /**
     * The current state of the Overlay Panel.
     */
    private PanelState mPanelState = PanelState.UNDEFINED;

    /**
     * Valid previous states for the Panel.
     */
    protected static final Map<PanelState, PanelState> PREVIOUS_STATES;
    static {
        Map<PanelState, PanelState> states = new HashMap<PanelState, PanelState>();
        // Pairs are of the form <Current, Previous>.
        states.put(PanelState.PEEKED, PanelState.CLOSED);
        states.put(PanelState.EXPANDED, PanelState.PEEKED);
        states.put(PanelState.MAXIMIZED, PanelState.EXPANDED);
        PREVIOUS_STATES = Collections.unmodifiableMap(states);
    }

    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * @param context The current Android {@link Context}.
     */
    public OverlayPanelBase(Context context) {
        mContext = context;
    }

    // ============================================================================================
    // General API
    // ============================================================================================

    /**
     * Animates the Overlay Panel to its closed state.
     * @param reason The reason for the change of panel state.
     * @param animate If the panel should animate closed.
     */
    protected abstract void closePanel(StateChangeReason reason, boolean animate);

    /**
     * Animates the acceptance of the Promo.
     */
    protected abstract void animatePromoAcceptance();

    /**
     * Animates the search term resolution.
     */
    protected abstract void animateSearchTermResolution();

    /**
     * Cancels the search term resolution animation if it is in progress.
     */
    protected abstract void cancelSearchTermResolutionAnimation();

    /**
     * Event notification that the Panel did get closed.
     * @param reason The reason the panel is closing.
     */
    protected abstract void onClosed(StateChangeReason reason);

    /**
     * @return The absolute amount in DP that the top controls have shifted off screen.
     */
    protected abstract float getTopControlsOffsetDp();

    /**
     * TODO(mdjones): This method should be removed from this class.
     * @return The resource id that contains how large the top controls are.
     */
    protected abstract int getControlContainerHeightResource();

    // ============================================================================================
    // Peeking promo methods
    // ============================================================================================

    /**
     * @return The height of the Peek Promo in the peeked state, in pixels.
     */
    protected float getPeekPromoHeightPeekingPx() {
        return 0;
    }

    /**
     * @return The height of the Peek Promo, in pixels.
     */
    protected float getPeekPromoHeight() {
        return 0;
    }

    // ============================================================================================
    // Layout Integration
    // ============================================================================================

    private float mLayoutWidth;
    private float mLayoutHeight;
    private boolean mIsToolbarShowing;

    private float mMaximumWidth;
    private float mMaximumHeight;

    private boolean mIsFullscreenSizePanelForTesting;
    private boolean mOverrideIsFullscreenSizePanelForTesting;

    /**
     * Called when the size of the view has changed.
     *
     * @param width  The new width in dp.
     * @param height The new width in dp.
     * @param isToolbarShowing Whether the Toolbar is showing.
     */
    public final void onSizeChanged(float width, float height, boolean isToolbarShowing) {
        mLayoutWidth = width;
        mLayoutHeight = height;
        mIsToolbarShowing = isToolbarShowing;

        mMaximumWidth = calculateOverlayPanelWidth();
        mMaximumHeight = getPanelHeightFromState(PanelState.MAXIMIZED);
    }

    /**
     * Overrides the FullscreenSizePanel state for testing.
     *
     * @param isFullscreenSizePanel
     */
    @VisibleForTesting
    public void setIsFullscreenSizePanelForTesting(boolean isFullscreenSizePanel) {
        mOverrideIsFullscreenSizePanelForTesting = true;
        mIsFullscreenSizePanelForTesting = isFullscreenSizePanel;
    }

    /**
     * @return Whether the Panel is in fullscreen size.
     */
    protected boolean isFullscreenSizePanel() {
        if (mOverrideIsFullscreenSizePanelForTesting) {
            return mIsFullscreenSizePanelForTesting;
        }
        return getFullscreenWidth() <= SMALL_PANEL_WIDTH_THRESHOLD_DP;
    }

    /**
     * @return Whether the narrow version of the Panel is supported, in any orientation.
     */
    protected boolean isNarrowSizePanelSupported() {
        return !isFullscreenSizePanel() || getFullscreenHeight() > SMALL_PANEL_WIDTH_THRESHOLD_DP;
    }

    /**
     * @return The current width of the Overlay Panel.
     */
    protected float calculateOverlayPanelWidth() {
        return isFullscreenSizePanel() ? getFullscreenWidth() : SMALL_PANEL_WIDTH_DP;
    }

    /**
     * @return The height of the Chrome toolbar in dp.
     */
    public float getToolbarHeight() {
        return mToolbarHeight;
    }

    /**
     * @param y The y coordinate.
     * @return The Y coordinate relative the fullscreen height.
     */
    public float getFullscreenY(float y) {
        if (mIsToolbarShowing) {
            y += mToolbarHeight / mPxToDp;
        }
        return y;
    }

    /**
     * @return Whether the Panel is showing.
     */
    public boolean isShowing() {
        return mIsShowing;
    }

    /**
     * @return Whether the Overlay Panel is opened. That is, whether it is EXPANDED or MAXIMIZED.
     */
    public boolean isPanelOpened() {
        return mPanelState == PanelState.EXPANDED || mPanelState == PanelState.MAXIMIZED;
    }

    /**
     * Starts showing the Panel.
     */
    protected void startShowing() {
        mIsShowing = true;
    }

    /**
     * @return The fullscreen width.
     */
    private float getFullscreenWidth() {
        return mLayoutWidth;
    }

    /**
     * @return The fullscreen height.
     */
    private float getFullscreenHeight() {
        // NOTE(mdjones): This value will always be the same for a particular orientation; it is
        // the content height + visible toolbar height.
        return mLayoutHeight + (getToolbarHeight() - getTopControlsOffsetDp());
    }

    /**
     * @return The maximum height of the Overlay Panel in dps.
     */
    public float getMaximumHeight() {
        return mMaximumHeight;
    }

    /**
     * @return The maximum width of the Overlay Panel in pixels.
     */
    public int getMaximumWidthPx() {
        return Math.round(mMaximumWidth / mPxToDp);
    }

    /**
     * @return The maximum height of the Overlay Panel in pixels.
     */
    public int getMaximumHeightPx() {
        return Math.round(mMaximumHeight / mPxToDp);
    }

    /**
     * The Panel Bar Container is a abstract container that groups the Controls
     * that will be visible when the Panel is in the peeked state.
     *
     * @return The Panel Bar Container in dps.
     */
    protected float getBarContainerHeight() {
        return getBarHeight() + getPeekPromoHeight();
    }

    /**
     * @return The width of the Overlay Panel Content View in pixels.
     */
    public int getContentViewWidthPx() {
        return getMaximumWidthPx();
    }

    /**
     * @return The height of the Overlay Panel Content View in pixels.
     */
    public int getContentViewHeightPx() {
        float barExpandedHeight = isFullscreenSizePanel()
                ? getToolbarHeight() : mBarHeightPeeking;
        return Math.round((mMaximumHeight - barExpandedHeight) / mPxToDp);
    }

    // ============================================================================================
    // UI States
    // ============================================================================================

    // --------------------------------------------------------------------------------------------
    // Test Infrastructure
    // --------------------------------------------------------------------------------------------

    /**
     * @param height The height of the Overlay Panel to be set.
     */
    @VisibleForTesting
    public void setHeightForTesting(float height) {
        mHeight = height;
    }

    /**
     * @param offsetY The vertical offset of the Overlay Panel to be
     *            set.
     */
    @VisibleForTesting
    public void setOffsetYForTesting(float offsetY) {
        mOffsetY = offsetY;
    }

    /**
     * @param isMaximized The setting for whether the Overlay Panel is fully
     *            maximized.
     */
    @VisibleForTesting
    public void setMaximizedForTesting(boolean isMaximized) {
        mIsMaximized = isMaximized;
    }

    /**
     * @param barHeight The height of the Overlay Bar to be set.
     */
    @VisibleForTesting
    public void setSearchBarHeightForTesting(float barHeight) {
        mBarHeight = barHeight;
    }

    /**
     * @param barBorderHeight The height of the Bar border to be set.
     */
    @VisibleForTesting
    public void setSearchBarBorderHeight(float barBorderHeight) {
        mBarBorderHeight = barBorderHeight;
    }

    // --------------------------------------------------------------------------------------------
    // Overlay Panel states
    // --------------------------------------------------------------------------------------------

    private float mOffsetX;
    private float mOffsetY;
    private float mHeight;
    private boolean mIsMaximized;

    /**
     * @return The vertical offset of the Overlay Panel.
     */
    public float getOffsetX() {
        return mOffsetX;
    }

    /**
     * @return The vertical offset of the Overlay Panel.
     */
    public float getOffsetY() {
        return mOffsetY;
    }

    /**
     * @return The width of the Overlay Panel in dps.
     */
    public float getWidth() {
        return mMaximumWidth;
    }

    /**
     * @return The height of the Overlay Panel in dps.
     */
    public float getHeight() {
        return mHeight;
    }

    /**
     * @return Whether the Overlay Panel is fully maximized.
     */
    public boolean isMaximized() {
        return mIsMaximized;
    }

    // --------------------------------------------------------------------------------------------
    // Panel Bar states
    // --------------------------------------------------------------------------------------------
    private float mBarMarginSide;
    private float mBarHeight;
    private boolean mIsBarBorderVisible;
    private float mBarBorderHeight;

    private boolean mBarShadowVisible = false;
    private float mBarShadowOpacity = 0.f;

    private float mArrowIconOpacity;

    private float mCloseIconOpacity;
    private float mCloseIconWidth;

    /**
     * @return The side margin of the Bar.
     */
    public float getBarMarginSide() {
        return mBarMarginSide;
    }

    /**
     * @return The height of the Bar.
     */
    public float getBarHeight() {
        return mBarHeight;
    }

    /**
     * @return Whether the Bar border is visible.
     */
    public boolean isBarBorderVisible() {
        return mIsBarBorderVisible;
    }

    /**
     * @return The height of the Bar border.
     */
    public float getBarBorderHeight() {
        return mBarBorderHeight;
    }

    /**
     * @return Whether the Bar shadow is visible.
     */
    public boolean getBarShadowVisible() {
        return mBarShadowVisible;
    }

    /**
     * @return The opacity of the Bar shadow.
     */
    public float getBarShadowOpacity() {
        return mBarShadowOpacity;
    }

    /**
     * @return The opacity of the arrow icon.
     */
    public float getArrowIconOpacity() {
        return mArrowIconOpacity;
    }

    /**
     * @return The rotation of the arrow icon, in degrees.
     */
    public float getArrowIconRotation() {
        return ARROW_ICON_ROTATION;
    }


    /**
     * @return The opacity of the close icon.
     */
    public float getCloseIconOpacity() {
        return mCloseIconOpacity;
    }

    /**
     * @return The width/height of the close icon.
     */
    public float getCloseIconDimension() {
        if (mCloseIconWidth == 0) {
            mCloseIconWidth = ApiCompatibilityUtils.getDrawable(mContext.getResources(),
                    CLOSE_ICON_DRAWABLE_ID).getIntrinsicWidth() * mPxToDp;
        }
        return mCloseIconWidth;
    }

    /**
     * @return The left X coordinate of the close icon.
     */
    public float getCloseIconX() {
        if (LocalizationUtils.isLayoutRtl()) {
            return getOffsetX() + getBarMarginSide();
        } else {
            return getOffsetX() + getWidth() - getBarMarginSide() - getCloseIconDimension();
        }
    }

    // --------------------------------------------------------------------------------------------
    // Base Page states
    // --------------------------------------------------------------------------------------------

    private float mBasePageY;
    private float mBasePageBrightness;

    /**
     * @return The vertical offset of the base page.
     */
    public float getBasePageY() {
        return mBasePageY;
    }

    /**
     * @return The brightness of the base page.
     */
    public float getBasePageBrightness() {
        return mBasePageBrightness;
    }

    /**
     * @return The color to fill the base page when viewport is resized/changes orientation.
     */
    public int getBasePageBackgroundColor() {
        // TODO(pedrosimonetti): Get the color from the CVC and apply a proper brightness transform.
        // NOTE(pedrosimonetti): Assumes the background color of the base page to be white (255)
        // and applies a simple brightness transformation based on the base page value.
        int value = Math.round(255 * mBasePageBrightness);
        value = MathUtils.clamp(value, 0, 255);
        return Color.rgb(value, value, value);
    }

    // --------------------------------------------------------------------------------------------
    // Progress Bar states
    // --------------------------------------------------------------------------------------------

    private float mProgressBarOpacity;
    private boolean mIsProgressBarVisible;
    private float mProgressBarHeight;
    private int mProgressBarCompletion;

    /**
     * @return Whether the Progress Bar is visible.
     */
    public boolean isProgressBarVisible() {
        return mIsProgressBarVisible;
    }

    /**
     * @param isVisible Whether the Progress Bar should be visible.
     */
    protected void setProgressBarVisible(boolean isVisible) {
        mIsProgressBarVisible = isVisible;
    }

    /**
     * @return The Progress Bar height.
     */
    public float getProgressBarHeight() {
        return mProgressBarHeight;
    }

    /**
     * @return The Progress Bar opacity.
     */
    public float getProgressBarOpacity() {
        return mProgressBarOpacity;
    }

    /**
     * @return The completion percentage of the Progress Bar.
     */
    public int getProgressBarCompletion() {
        return mProgressBarCompletion;
    }

    /**
     * @param completion The completion percentage to be set.
     */
    protected void setProgressBarCompletion(int completion) {
        mProgressBarCompletion = completion;
    }

    // --------------------------------------------------------------------------------------------
    // Promo states
    // --------------------------------------------------------------------------------------------

    private boolean mPromoVisible = false;
    private float mPromoContentHeightPx = 0.f;
    private float mPromoHeightPx;
    private float mPromoOpacity;

    /**
     * @return Whether the promo is visible.
     */
    public boolean getPromoVisible() {
        return mPromoVisible;
    }

    /**
     * Sets the height of the promo content.
     */
    public void setPromoContentHeightPx(float heightPx) {
        mPromoContentHeightPx = heightPx;
    }

    /**
     * @return Height of the promo in dps.
     */
    public float getPromoHeight() {
        return mPromoHeightPx * mPxToDp;
    }

    /**
     * @return Height of the promo in pixels.
     */
    public float getPromoHeightPx() {
        return mPromoHeightPx;
    }

    /**
     * @return The opacity of the promo.
     */
    public float getPromoOpacity() {
        return mPromoOpacity;
    }

    /**
     * @return Y coordinate of the promo in pixels.
     */
    protected float getPromoYPx() {
        return Math.round((getOffsetY() + getBarContainerHeight()) / mPxToDp);
    }

    // ============================================================================================
    // State Handler
    // ============================================================================================

    /**
     * @return The panel's state.
     */
    public PanelState getPanelState() {
        return mPanelState;
    }

    /**
     * @return If the the panel supports an EXPANDED state rather than just PEEKING or MAXIMIZED.
     */
    public boolean supportsExpandedState() {
        return true;
    }

    /**
     * @return The {@code PanelState} that is before the |state| in the order of states.
     */
    public PanelState getPreviousPanelState(PanelState state) {
        PanelState prevState = PREVIOUS_STATES.get(state);
        if (prevState == PanelState.EXPANDED && !supportsExpandedState()) {
            prevState = PREVIOUS_STATES.get(prevState);
        }
        return prevState != null ? prevState : PanelState.UNDEFINED;
    }

    /**
     * Sets the panel's state.
     * @param state The panel state to transition to.
     * @param reason The reason for a change in the panel's state.
     */
    public void setPanelState(PanelState state, StateChangeReason reason) {
        if (state == PanelState.CLOSED) {
            mIsShowing = false;
            onClosed(reason);
        } else if (state == PanelState.EXPANDED) {
            showPromoViewAtYPosition(getPromoYPx());
        }

        // We should only set the state at the end of this method, in oder to make sure that
        // all callbacks will be fired before changing the state of the Panel. This prevents
        // some flakiness on tests since they rely on changes of state to determine when a
        // particular action has been completed.
        mPanelState = state;
    }

    /**
     * Determine if a specific {@code PanelState} is a valid state in the current environment.
     * @param state The state being evaluated.
     * @return whether the state is valid.
     */
    public boolean isValidState(PanelState state) {
        // EXPANDED is not a valid state if a panel implementation doesn't support it.
        if (!supportsExpandedState() && state == PanelState.EXPANDED) return false;
        // MAXIMIZED is not the previous state of anything, but it's a valid state.
        return PREVIOUS_STATES.values().contains(state) || state == PanelState.MAXIMIZED;
    }

    // ============================================================================================
    // Helpers
    // ============================================================================================

    /**
     * Initializes the UI state.
     */
    protected void initializeUiState() {
        mIsShowing = false;

        // Static values.
        mPxToDp = 1.f / mContext.getResources().getDisplayMetrics().density;

        mToolbarHeight = mContext.getResources().getDimension(
                getControlContainerHeightResource()) * mPxToDp;

        mBarHeightPeeking = mContext.getResources().getDimension(
                R.dimen.overlay_panel_bar_height) * mPxToDp;
        mBarHeightMaximized = mContext.getResources().getDimension(
                R.dimen.toolbar_height_no_shadow) * mPxToDp;
        mBarHeightExpanded =
                Math.round((mBarHeightPeeking + mBarHeightMaximized) / 2.f);
        mBarMarginSide = BAR_ICON_SIDE_PADDING_DP;
        mProgressBarHeight = PROGRESS_BAR_HEIGHT_DP;
        mBarBorderHeight = BAR_BORDER_HEIGHT_DP;

        // Dynamic values.
        mBarHeight = mBarHeightPeeking;
    }

    /**
     * Gets the height of the Overlay Panel in dps for a given |state|.
     *
     * @param state The state whose height will be calculated.
     * @return The height of the Overlay Panel in dps for a given |state|.
     */
    protected float getPanelHeightFromState(PanelState state) {
        float fullscreenHeight = getFullscreenHeight();
        float panelHeight = 0;

        if (state == PanelState.UNDEFINED) {
            panelHeight = 0;
        } else if (state == PanelState.CLOSED) {
            panelHeight = 0;
        } else if (state == PanelState.PEEKED) {
            panelHeight = mBarHeightPeeking + getPeekPromoHeightPeekingPx() * mPxToDp;
        } else if (state == PanelState.EXPANDED) {
            if (isFullscreenSizePanel()) {
                panelHeight = fullscreenHeight * EXPANDED_PANEL_HEIGHT_PERCENTAGE;
            } else {
                panelHeight = (fullscreenHeight - mToolbarHeight)
                        * EXPANDED_PANEL_HEIGHT_PERCENTAGE;
            }
        } else if (state == PanelState.MAXIMIZED) {
            if (isFullscreenSizePanel()) {
                panelHeight = fullscreenHeight;
            } else {
                panelHeight = fullscreenHeight - mToolbarHeight;
            }
        }

        return panelHeight;
    }

    /**
     * Finds the state which has the nearest height compared to a given
     * |desiredPanelHeight|.
     *
     * @param desiredPanelHeight The height to compare to.
     * @return The nearest panel state.
     */
    protected PanelState findNearestPanelStateFromHeight(float desiredPanelHeight) {
        PanelState closestPanelState = PanelState.CLOSED;
        float smallestHeightDiff = Float.POSITIVE_INFINITY;

        // Iterate over all states and find the one which has the nearest
        // height.
        for (PanelState state : PanelState.values()) {
            if (!isValidState(state)) {
                continue;
            }

            float height = getPanelHeightFromState(state);
            float heightDiff = Math.abs(desiredPanelHeight - height);
            if (heightDiff < smallestHeightDiff) {
                closestPanelState = state;
                smallestHeightDiff = heightDiff;
            }
        }

        return closestPanelState;
    }

    /**
     * Sets the last panel height within the limits allowable by our UI.
     *
     * @param height The height of the panel in dps.
     */
    protected void setClampedPanelHeight(float height) {
        final float clampedHeight = MathUtils.clamp(height,
                getPanelHeightFromState(PanelState.MAXIMIZED),
                getPanelHeightFromState(PanelState.PEEKED));
        setPanelHeight(clampedHeight);
    }

    /**
     * Sets the panel height.
     *
     * @param height The height of the panel in dps.
     */
    protected void setPanelHeight(float height) {
        // As soon as we resize the Panel to a different height than the expanded one,
        // then we should hide the Promo View once the snapshot will be shown in its place.
        if (height != getPanelHeightFromState(PanelState.EXPANDED)) {
            hidePromoView();
        }

        updatePanelForHeight(height);
    }

    /**
     * @param state The Panel state.
     * @return Whether the Panel height matches the one from the given state.
     */
    protected boolean doesPanelHeightMatchState(PanelState state) {
        return state == getPanelState() && getHeight() == getPanelHeightFromState(state);
    }

    // ============================================================================================
    // UI Update Handling
    // ============================================================================================

    /**
     * Updates the UI state for a given |height|.
     *
     * @param height The Overlay Panel height.
     */
    private void updatePanelForHeight(float height) {
        PanelState endState = findLargestPanelStateFromHeight(height);
        PanelState startState = getPreviousPanelState(endState);
        float percentage = getStateCompletion(height, startState, endState);

        updatePanelSize(height, endState, percentage);

        if (endState == PanelState.CLOSED || endState == PanelState.PEEKED) {
            updatePanelForCloseOrPeek(percentage);
        } else if (endState == PanelState.EXPANDED) {
            updatePanelForExpansion(percentage);
        } else if (endState == PanelState.MAXIMIZED) {
            updatePanelForMaximization(percentage);
        }
    }

    /**
     * Updates the Panel size information.
     *
     * @param height The Overlay Panel height.
     * @param endState The final state of transition being executed.
     * @param percentage The completion percentage of the transition.
     */
    private void updatePanelSize(float height, PanelState endState, float percentage) {
        mHeight = height;
        mOffsetX = isFullscreenSizePanel() ? 0.f
                : Math.round((getFullscreenWidth() - calculateOverlayPanelWidth()) / 2.f);
        mOffsetY = getFullscreenHeight() - mHeight;
        mIsMaximized = height == getPanelHeightFromState(PanelState.MAXIMIZED);
    }

    /**
     * Finds the largest Panel state which is being transitioned to/from.
     * Whenever the Panel is in between states, let's say, when resizing the
     * Panel from its peeked to expanded state, we need to know those two states
     * in order to calculate how closely we are from one of them. This method
     * will always return the nearest state with the largest height, and
     * together with the state preceding it, it's possible to calculate how far
     * the Panel is from them.
     *
     * @param panelHeight The height to compare to.
     * @return The panel state which is being transitioned to/from.
     */
    private PanelState findLargestPanelStateFromHeight(float panelHeight) {
        PanelState stateFound = PanelState.CLOSED;

        // Iterate over all states and find the largest one which is being
        // transitioned to/from.
        for (PanelState state : PanelState.values()) {
            if (!isValidState(state)) {
                continue;
            }
            if (panelHeight <= getPanelHeightFromState(state)) {
                stateFound = state;
                break;
            }
        }

        return stateFound;
    }

    /**
     * Gets the state completion percentage, taking into consideration the |height| of the Overlay
     * Panel, and the initial and final states. A completion of 0 means the Panel is in the initial
     * state and a completion of 1 means the Panel is in the final state.
     *
     * @param height The height of the Overlay Panel.
     * @param startState The initial state of the Panel.
     * @param endState The final state of the Panel.
     * @return The completion percentage.
     */
    private float getStateCompletion(float height, PanelState startState, PanelState endState) {
        float startSize = getPanelHeightFromState(startState);
        float endSize = getPanelHeightFromState(endState);
        float percentage =
                // NOTE(pedrosimonetti): Handle special case from PanelState.UNDEFINED
                // to PanelState.CLOSED, where both have a height of zero. Returning
                // zero here means the Panel will be reset to its CLOSED state.
                startSize == 0.f && endSize == 0.f ? 0.f
                        : (height - startSize) / (endSize - startSize);

        return percentage;
    }

    /**
     * Updates the UI state for the closed to peeked transition (and vice
     * versa), according to a completion |percentage|.
     *
     * @param percentage The completion percentage.
     */
    protected void updatePanelForCloseOrPeek(float percentage) {
        // Update the opt out promo.
        updatePromoVisibility(1.f);

        // Base page offset.
        mBasePageY = 0.f;

        // Base page brightness.
        mBasePageBrightness = BASE_PAGE_BRIGHTNESS_STATE_PEEKED;

        // Bar height.
        mBarHeight = mBarHeightPeeking;

        // Bar border.
        mIsBarBorderVisible = false;

        // Arrow Icon.
        mArrowIconOpacity = ARROW_ICON_OPACITY_STATE_PEEKED;

        // Close icon opacity.
        mCloseIconOpacity = CLOSE_ICON_OPACITY_STATE_PEEKED;

        // Progress Bar.
        mProgressBarOpacity = 0.f;

        // Update the Bar Shadow.
        updateBarShadow();
    }

    /**
     * Updates the UI state for the peeked to expanded transition (and vice
     * versa), according to a completion |percentage|.
     *
     * @param percentage The completion percentage.
     */
    protected void updatePanelForExpansion(float percentage) {
        // Update the opt out promo.
        updatePromoVisibility(1.f);

        // Base page offset.
        float baseBaseY = MathUtils.interpolate(
                0.f,
                getBasePageTargetY(),
                percentage);
        mBasePageY = baseBaseY;

        // Base page brightness.
        float brightness = MathUtils.interpolate(
                BASE_PAGE_BRIGHTNESS_STATE_PEEKED,
                BASE_PAGE_BRIGHTNESS_STATE_EXPANDED,
                percentage);
        mBasePageBrightness = brightness;

        // Bar height.
        float barHeight = Math.round(MathUtils.interpolate(
                mBarHeightPeeking,
                getBarHeightExpanded(),
                percentage));
        mBarHeight = barHeight;

        // Bar border.
        mIsBarBorderVisible = true;

        // Determine fading element opacities. The arrow icon needs to finish fading out before
        // the close icon starts fading in. Any other elements fading in or fading out should use
        // the same percentage.
        float fadingOutPercentage = Math.min(percentage, .5f) / .5f;
        float fadingInPercentage = Math.max(percentage - .5f, 0.f) / .5f;

        // Arrow Icon.
        mArrowIconOpacity = MathUtils.interpolate(
                ARROW_ICON_OPACITY_STATE_PEEKED,
                ARROW_ICON_OPACITY_STATE_EXPANDED,
                fadingOutPercentage);

        // Close Icon.
        mCloseIconOpacity = MathUtils.interpolate(
                CLOSE_ICON_OPACITY_STATE_PEEKED,
                CLOSE_ICON_OPACITY_STATE_EXPANDED,
                fadingInPercentage);

        // Progress Bar.
        float peekedHeight = getPanelHeightFromState(PanelState.PEEKED);
        float threshold = PROGRESS_BAR_VISIBILITY_THRESHOLD_DP / mPxToDp;
        float diff = Math.min(mHeight - peekedHeight, threshold);
        // Fades the Progress Bar the closer it gets to the bottom of the
        // screen.
        float progressBarOpacity = MathUtils.interpolate(0.f, 1.f, diff / threshold);
        mProgressBarOpacity = progressBarOpacity;

        // Update the Bar Shadow.
        updateBarShadow();
    }

    /**
     * Updates the UI state for the expanded to maximized transition (and vice
     * versa), according to a completion |percentage|.
     *
     * @param percentage The completion percentage.
     */
    protected void updatePanelForMaximization(float percentage) {
        // Update the opt out promo.
        updatePromoVisibility(1.f - percentage);

        // Base page offset.
        float startTargetY = supportsExpandedState() ? getBasePageTargetY() : 0.0f;
        mBasePageY = MathUtils.interpolate(
                startTargetY,
                getBasePageTargetY(),
                percentage);

        // Base page brightness.
        float startBrightness = supportsExpandedState()
                ? BASE_PAGE_BRIGHTNESS_STATE_EXPANDED : BASE_PAGE_BRIGHTNESS_STATE_PEEKED;
        mBasePageBrightness = MathUtils.interpolate(
                startBrightness,
                BASE_PAGE_BRIGHTNESS_STATE_MAXIMIZED,
                percentage);

        // Bar height.
        float startBarHeight = supportsExpandedState()
                ? getBarHeightExpanded() : getBarHeightPeeking();
        mBarHeight = Math.round(MathUtils.interpolate(
                startBarHeight,
                getBarHeightMaximized(),
                percentage));

        // Bar border.
        mIsBarBorderVisible = true;

        // Arrow Icon.
        mArrowIconOpacity = ARROW_ICON_OPACITY_STATE_MAXIMIZED;

        // Close Icon.
        mCloseIconOpacity = CLOSE_ICON_OPACITY_STATE_MAXIMIZED;

        // Progress Bar.
        mProgressBarOpacity = 1.f;

        // Update the Bar Shadow.
        updateBarShadow();
    }

    private float getBarHeightExpanded() {
        if (isFullscreenSizePanel()) {
            return mBarHeightExpanded;
        } else {
            return mBarHeightPeeking;
        }
    }

    private float getBarHeightMaximized() {
        if (isFullscreenSizePanel()) {
            return mBarHeightMaximized;
        } else {
            return mBarHeightPeeking;
        }
    }

    /**
     * @return The peeking height of the panel's bar in dp.
     */
    protected float getBarHeightPeeking() {
        return mBarHeightPeeking;
    }

    /**
     * @return Whether the Panel Promo is visible.
     */
    protected boolean isPromoVisible() {
        return false;
    }

    /**
     * Updates the UI state for Opt Out Promo.
     *
     * @param percentage The visibility percentage of the Promo. A visibility of 0 means the
     * Promo is not visible. A visibility of 1 means the Promo is fully visible. And
     * visibility between 0 and 1 means the Promo is partially visible.
     */
    private void updatePromoVisibility(float percentage) {
        if (isPromoVisible()) {
            mPromoVisible = true;

            mPromoHeightPx = Math.round(MathUtils.clamp(percentage * mPromoContentHeightPx,
                    0.f, mPromoContentHeightPx));
            mPromoOpacity = percentage;
        } else {
            mPromoVisible = false;
            mPromoHeightPx = 0.f;
            mPromoOpacity = 0.f;
        }
    }

    /**
     * Updates the UI state for Bar Shadow.
     */
    public void updateBarShadow() {
        float barShadowHeightPx = 9.f / mPxToDp;
        if (mPromoVisible && mPromoHeightPx > 0.f) {
            mBarShadowVisible = true;
            float threshold = 2 * barShadowHeightPx;
            mBarShadowOpacity = mPromoHeightPx > barShadowHeightPx ? 1.f
                    : MathUtils.interpolate(0.f, 1.f, mPromoHeightPx / threshold);
        } else {
            mBarShadowVisible = false;
            mBarShadowOpacity = 0.f;
        }
    }

    // ============================================================================================
    // Base Page Offset
    // ============================================================================================

    /**
     * Updates the coordinate of the existing selection.
     * @param y The y coordinate of the selection in pixels.
     */
    protected void updateBasePageSelectionYPx(float y) {
        mBasePageSelectionYPx = y;
        updateBasePageTargetY();
    }

    /**
     * Updates the target offset of the Base Page in order to keep the selection in view
     * after expanding the Panel.
     */
    protected void updateBasePageTargetY() {
        mBasePageTargetY = calculateBasePageTargetY(PanelState.EXPANDED);
    }

    /**
     * Calculates the target offset of the Base Page in order to keep the selection in view
     * after expanding the Panel to the given |expandedState|.
     *
     * @param expandedState
     * @return The target offset Y.
     */
    protected float calculateBasePageTargetY(PanelState expandedState) {
        // Only a fullscreen wide Panel should offset the base page. A small panel should
        // always return zero to ensure the Base Page remains in the same position.
        if (!isFullscreenSizePanel()) return 0.f;

        // Convert from px to dp.
        final float selectionY = mBasePageSelectionYPx * mPxToDp;

        // Calculate the exact height of the expanded Panel without taking into
        // consideration the height of the shadow (what is returned by the
        // getPanelFromHeight method). We need the measurement of the portion
        // of the Panel that occludes the page.
        final float expandedHeight = getPanelHeightFromState(expandedState);

        // Calculate the offset to center the selection on the available area.
        final float fullscreenHeight = getFullscreenHeight();
        final float availableHeight = fullscreenHeight - expandedHeight;
        float offset = -selectionY + availableHeight / 2;

        // Make sure offset is negative to prevent Base Page from moving down,
        // because there's nothing to render above the Page.
        offset = Math.min(offset, 0.f);
        // If visible, the Toolbar will be hidden. Therefore, we need to adjust
        // the offset to account for this difference.
        if (mIsToolbarShowing) offset -= mToolbarHeight;
        // Make sure the offset is not greater than the expanded height, because
        // there's nothing to render below the Page.
        offset = Math.max(offset, -expandedHeight);

        return offset;
    }

    /**
     * @return The Y coordinate to apply to the Base Page in order to keep the selection
     *         in view when the Overlay Panel is in EXPANDED state.
     */
    public float getBasePageTargetY() {
        return mBasePageTargetY;
    }

    // ============================================================================================
    // Resource Loader
    // ============================================================================================

    protected ViewGroup mContainerView;
    protected DynamicResourceLoader mResourceLoader;

    /**
     * @param resourceLoader The {@link DynamicResourceLoader} to register and unregister the view.
     */
    public void setDynamicResourceLoader(DynamicResourceLoader resourceLoader) {
        mResourceLoader = resourceLoader;

        if (mPromoView != null) {
            mResourceLoader.registerResource(R.id.contextual_search_opt_out_promo,
                    mPromoView.getResourceAdapter());
        }
    }

    /**
     * Sets the container ViewGroup to which the auxiliary Views will be attached to.
     *
     * @param container The {@link ViewGroup} container.
     */
    public void setContainerView(ViewGroup container) {
        mContainerView = container;
    }

    // ============================================================================================
    // Promo Host
    // ============================================================================================

    @Override
    public void onPromoPreferenceClick() {
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                PreferencesLauncher.launchSettingsPage(mContext,
                        ContextualSearchPreferenceFragment.class.getName());
            }
        });
    }

    /**
     * @param enabled Whether the preference should be enabled.
     */
    public void setPreferenceState(boolean enabled) {
    }

    @Override
    public void onPromoButtonClick(boolean accepted) {
        if (accepted) {
            animatePromoAcceptance();
        } else {
            hidePromoView();
            // NOTE(pedrosimonetti): If the user has opted out of Contextual Search, we should set
            // the preference right away because the preference state controls whether the promo
            // will be visible, and we want to hide the promo immediately when the user opts out.
            setPreferenceState(false);
            closePanel(StateChangeReason.OPTOUT, true);
        }
    }

    // ============================================================================================
    // Opt Out Promo View
    // ============================================================================================

    // TODO(pedrosimonetti): Consider maybe adding a 9.patch to avoid the hacky nested layouts in
    // order to have the transparent gap at the top of the Promo View.

    /**
     * The {@link ContextualSearchOptOutPromo} instance.
     */
    private ContextualSearchOptOutPromo mPromoView;

    /**
     * Whether the Search Promo View is visible.
     */
    private boolean mIsSearchPromoViewVisible = false;

    /**
     * Creates the Search Promo View.
     */
    protected void createPromoView() {
        if (isPromoVisible() && mPromoView == null) {
            assert mContainerView != null;

            // TODO(pedrosimonetti): Refactor promo code to use ViewResourceInflater.
            LayoutInflater.from(mContext).inflate(
                    R.layout.contextual_search_opt_out_promo, mContainerView);
            mPromoView = (ContextualSearchOptOutPromo)
                    mContainerView.findViewById(R.id.contextual_search_opt_out_promo);

            final int maximumWidth = getMaximumWidthPx();

            // Adjust size for small Panel.
            if (!isFullscreenSizePanel()) {
                mPromoView.getLayoutParams().width = maximumWidth;
                mPromoView.requestLayout();
            }

            if (mResourceLoader != null) {
                mResourceLoader.registerResource(R.id.contextual_search_opt_out_promo,
                        mPromoView.getResourceAdapter());
            }

            mPromoView.setPromoHost(this);
            setPromoContentHeightPx(mPromoView.getHeightForGivenWidth(maximumWidth));
        }

        assert mPromoView != null;
    }

    /**
     * Destroys the Search Promo View.
     */
    protected void destroyPromoView() {
        if (mPromoView != null) {
            mContainerView.removeView(mPromoView);
            mPromoView = null;
            if (mResourceLoader != null) {
                mResourceLoader.unregisterResource(R.id.contextual_search_opt_out_promo);
            }
        }
    }

    /**
     * Displays the Search Promo View at the given Y position.
     *
     * @param y The Y position.
     */
    public void showPromoViewAtYPosition(float y) {
        if (mPromoView == null || !isPromoVisible()) return;

        float offsetX = getOffsetX() / mPxToDp;
        if (LocalizationUtils.isLayoutRtl()) {
            offsetX = -offsetX;
        }

        mPromoView.setTranslationX(offsetX);
        mPromoView.setTranslationY(y);
        mPromoView.setVisibility(View.VISIBLE);

        // NOTE(pedrosimonetti): We need to call requestLayout, otherwise
        // the Promo View will not become visible.
        mPromoView.requestLayout();

        mIsSearchPromoViewVisible = true;
    }

    /**
     * Hides the Search Promo View.
     */
    public void hidePromoView() {
        if (mPromoView == null
                || !mIsSearchPromoViewVisible
                || !isPromoVisible()) {
            return;
        }

        mPromoView.setVisibility(View.INVISIBLE);

        mIsSearchPromoViewVisible = false;
    }

    /**
     * Updates the UI state for Opt In Promo animation.
     *
     * @param percentage The visibility percentage of the Promo.
     */
    protected void setPromoVisibilityForOptInAnimation(float percentage) {
        updatePromoVisibility(percentage);
        updateBarShadow();
    }
}
