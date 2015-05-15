// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchOptOutPromo.ContextualSearchPromoHost;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel.StateChangeReason;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.privacy.ContextualSearchPreferenceFragment;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Base abstract class for the Contextual Search Panel.
 */
abstract class ContextualSearchPanelBase extends ContextualSearchPanelStateHandler
        implements ContextualSearchPromoHost {

    /**
     * The height of the expanded Contextual Search Panel relative to the height
     * of the screen.
     */
    private static final float EXPANDED_PANEL_HEIGHT_PERCENTAGE = .7f;

    /**
     * The height of the Toolbar in dps.
     */
    private static final float TOOLBAR_HEIGHT_DP = 56.f;

    /**
     * The height of the Contextual Search Panel's Shadow in dps.
     */
    private static final float PANEL_SHADOW_HEIGHT_DP = 16.f;

    /**
     * The brightness of the base page when the Panel is peeking.
     */
    private static final float BASE_PAGE_BRIGHTNESS_STATE_PEEKED = 1.f;

    /**
     * The brightness of the base page when the Panel is expanded.
     */
    private static final float BASE_PAGE_BRIGHTNESS_STATE_EXPANDED = .7f;

    /**
     * The brightness of the base page when the Panel is maximized.
     */
    private static final float BASE_PAGE_BRIGHTNESS_STATE_MAXIMIZED = .3f;

    /**
     * The opacity of the search provider icon when the Panel is peeking.
     */
    private static final float SEARCH_PROVIDER_ICON_OPACITY_STATE_PEEKED = 1.f;

    /**
     * The opacity of the search provider icon when the Panel is expanded.
     */
    private static final float SEARCH_PROVIDER_ICON_OPACITY_STATE_EXPANDED = 1.f;

    /**
     * The opacity of the search provider icon when the Panel is maximized.
     */
    private static final float SEARCH_PROVIDER_ICON_OPACITY_STATE_MAXIMIZED = 0.f;

    /**
     * The opacity of the search icon when the Panel is peeking.
     */
    private static final float SEARCH_ICON_OPACITY_PEEKED = 0.f;

    /**
     * The opacity of the search icon when the Panel is expanded.
     */
    private static final float SEARCH_ICON_OPACITY_EXPANDED = 0.f;

    /**
     * The opacity of the search icon when the Panel is maximized.
     */
    private static final float SEARCH_ICON_OPACITY_MAXIMIZED = 1.f;

    /**
     * The margin top of the Contextual Search Bar in dps.
     */
    private static final float SEARCH_BAR_MARGIN_TOP_DP = 16.f;

    /**
     * The padding left of the Search Icon in dps.
     */
    private static final float SEARCH_ICON_PADDING_LEFT_DP = 16.f;

    /**
     * The height of the Search Bar's border in dps.
     */
    private static final float SEARCH_BAR_BORDER_HEIGHT_DP = 1.f;

    /**
     * The height of the Progress Bar in dps.
     */
    private static final float PROGRESS_BAR_HEIGHT_DP = 2.f;

    /**
     * The distance from the Progress Bar must be away from the bottom of the
     * screen in order to be completely visible. The closer the Progress Bar
     * gets to the bottom of the screen, the lower its opacity will be. When the
     * Progress Bar is at the very bottom of the screen (when the Search Panel
     * is peeking) it will be completely invisible.
     */
    private static final float PROGRESS_BAR_VISIBILITY_THRESHOLD_DP = 10.f;

    /**
     * The height of the Search Bar when the Panel is peeking, in dps.
     */
    private final float mSearchBarHeightPeeking;

    /**
     * The height of the Search Bar when the Panel is expanded, in dps.
     */
    private final float mSearchBarHeightExpanded;

    /**
     * The height of the Search Bar when the Panel is maximized, in dps.
     */
    private final float mSearchBarHeightMaximized;

    /**
     * Ratio of dps per pixel.
     */
    private final float mPxToDp;

    /**
     * The approximate Y coordinate of the selection in pixels.
     */
    private float mBasePageSelectionYPx = -1.f;

    /**
     * The Y coordinate to apply to the Base Page in order to keep the selection
     * in view when the Search Panel is in its EXPANDED state.
     */
    private float mBasePageTargetY = 0.f;

    /**
     * Whether the Panel is showing.
     */
    private boolean mIsShowing;

    /**
     * The current context.
     */
    private final Context mContext;

    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * @param context The current Android {@link Context}.
     */
    public ContextualSearchPanelBase(Context context) {
        mContext = context;

        mPxToDp = 1.f / context.getResources().getDisplayMetrics().density;

        mSearchBarHeightPeeking = context.getResources().getDimension(
                R.dimen.contextual_search_bar_height) * mPxToDp;
        mSearchBarHeightMaximized = TOOLBAR_HEIGHT_DP + PANEL_SHADOW_HEIGHT_DP;
        mSearchBarHeightExpanded =
                Math.round((mSearchBarHeightPeeking + mSearchBarHeightMaximized) / 2.f);

        initializeUiState();
    }

    // ============================================================================================
    // General API
    // ============================================================================================

    /**
     * Animates the Contextual Search Panel to its closed state.
     *
     * @param reason The reason for the change of panel state.
     */
    protected abstract void closePanel(StateChangeReason reason, boolean animate);

    /**
     * Sets Contextual Search's preference state.
     *
     * @param enabled Whether the preference should be enabled.
     */
    public abstract void setPreferenceState(boolean enabled);

    /**
     * @return Whether the Panel Promo is available.
     */
    protected abstract boolean isPromoAvailable();

    /**
     * Animates the acceptance of the Promo.
     */
    protected abstract void animatePromoAcceptance();

    /**
     * Event notification that the Panel did get closed.
     */
    protected abstract void onClose();

    // ============================================================================================
    // Layout Integration
    // ============================================================================================

    private float mLayoutWidth;
    private float mLayoutHeight;
    private boolean mIsToolbarShowing;

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
    }

    /**
     * Returns the Y position of the Contextual Search Panel.
     * Layouts supporting Contextual Search should override this method.
     *
     * @return The current Y-position of the Contextual Search Panel.
     */
    public float getContextualSearchPanelY() {
        return getFullscreenHeight() - mHeight;
    }

    /**
     * @param y The y coordinate.
     * @return The Y coordinate relative the fullscreen height.
     */
    public float getFullscreenY(float y) {
        if (mIsToolbarShowing) {
            y += TOOLBAR_HEIGHT_DP / mPxToDp;
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
        float height = mLayoutHeight;
        // NOTE(pedrosimonetti): getHeight() only returns the content height
        // when the Toolbar is not showing. If we don't add the Toolbar height
        // here, there will be a "jump" when swiping the Search Panel around.
        // TODO(pedrosimonetti): Find better way to get the fullscreen height.
        if (mIsToolbarShowing) {
            height += TOOLBAR_HEIGHT_DP;
        }
        return height;
    }

    // ============================================================================================
    // UI States
    // ============================================================================================

    // --------------------------------------------------------------------------------------------
    // Test Infrastructure
    // --------------------------------------------------------------------------------------------

    /**
     * @param offsetY The vertical offset of the Contextual Search Panel to be
     *            set.
     */
    @VisibleForTesting
    public void setOffsetYForTesting(float offsetY) {
        mOffsetY = offsetY;
    }

    /**
     * @param isMaximized The setting for whether the Search Panel is fully
     *            maximized.
     */
    @VisibleForTesting
    public void setMaximizedForTesting(boolean isMaximized) {
        mIsMaximized = isMaximized;
    }

    /**
     * @param searchBarHeight The height of the Contextual Search Bar to be set.
     */
    @VisibleForTesting
    public void setSearchBarHeightForTesting(float searchBarHeight) {
        mSearchBarHeight = searchBarHeight;
    }

    /**
     * @param searchBarBorderHeight The height of the Search Bar border to be
     *            set.
     */
    @VisibleForTesting
    public void setSearchBarBorderHeight(float searchBarBorderHeight) {
        mSearchBarBorderHeight = searchBarBorderHeight;
    }

    // --------------------------------------------------------------------------------------------
    // Contextual Search Panel states
    // --------------------------------------------------------------------------------------------

    private float mOffsetY;
    private float mHeight;
    private float mWidth;
    private boolean mIsMaximized;

    /**
     * @return The vertical offset of the Contextual Search Panel.
     */
    public float getOffsetY() {
        return mOffsetY;
    }

    /**
     * @return The height of the Contextual Search Panel.
     */
    public float getHeight() {
        return mHeight;
    }

    /**
     * @return The width of the Contextual Search Panel.
     */
    public float getWidth() {
        return mWidth;
    }

    /**
     * @return Whether the Search Panel is fully maximized.
     */
    public boolean isMaximized() {
        return mIsMaximized;
    }

    // --------------------------------------------------------------------------------------------
    // Contextual Search Bar states
    // --------------------------------------------------------------------------------------------

    private float mSearchBarMarginTop;
    private float mSearchBarHeight;
    private float mSearchBarTextOpacity;
    private boolean mIsSearchBarBorderVisible;
    private float mSearchBarBorderY;
    private float mSearchBarBorderHeight;

    boolean mSearchBarShadowVisible = false;
    float mSearchBarShadowOpacity = 0.f;

    private float mSearchProviderIconOpacity;
    private float mSearchIconPaddingLeft;
    private float mSearchIconOpacity;

    /**
     * @return The top margin of the Contextual Search Bar.
     */
    public float getSearchBarMarginTop() {
        return mSearchBarMarginTop;
    }

    /**
     * @return The height of the Contextual Search Bar.
     */
    public float getSearchBarHeight() {
        return mSearchBarHeight;
    }

    /**
     * @return The opacity of the Contextual Search Bar text.
     */
    public float getSearchBarTextOpacity() {
        return mSearchBarTextOpacity;
    }

    /**
     * @return Whether the Search Bar border is visible.
     */
    public boolean isSearchBarBorderVisible() {
        return mIsSearchBarBorderVisible;
    }

    /**
     * @return The Y coordinate of the Search Bar border.
     */
    public float getSearchBarBorderY() {
        return mSearchBarBorderY;
    }

    /**
     * @return The height of the Search Bar border.
     */
    public float getSearchBarBorderHeight() {
        return mSearchBarBorderHeight;
    }

    /**
     * @return Whether the Search Bar shadow is visible.
     */
    public boolean getSearchBarShadowVisible() {
        return mSearchBarShadowVisible;
    }

    /**
     * @return The opacity of the Search Bar shadow.
     */
    public float getSearchBarShadowOpacity() {
        return mSearchBarShadowOpacity;
    }

    /**
     * @return The opacity of the search provider's icon.
     */
    public float getSearchProviderIconOpacity() {
        return mSearchProviderIconOpacity;
    }

    /**
     * @return The left padding of the search icon.
     */
    public float getSearchIconPaddingLeft() {
        return mSearchIconPaddingLeft;
    }

    /**
     * @return The opacity of the search icon.
     */
    public float getSearchIconOpacity() {
        return mSearchIconOpacity;
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

    // --------------------------------------------------------------------------------------------
    // Progress Bar states
    // --------------------------------------------------------------------------------------------

    private float mProgressBarOpacity;
    private boolean mIsProgressBarVisible;
    private float mProgressBarY;
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
     * @return The Y coordinate of the Progress Bar.
     */
    public float getProgressBarY() {
        return mProgressBarY;
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
        return Math.round((getOffsetY() + getSearchBarHeight()) / mPxToDp);
    }

    // ============================================================================================
    // Helpers
    // ============================================================================================

    /**
     * Initializes the UI state.
     */
    private void initializeUiState() {
        mIsShowing = false;

        // Static values.
        mSearchBarMarginTop = SEARCH_BAR_MARGIN_TOP_DP;
        mSearchIconPaddingLeft = SEARCH_ICON_PADDING_LEFT_DP;
        mProgressBarHeight = PROGRESS_BAR_HEIGHT_DP;
        mSearchBarBorderHeight = SEARCH_BAR_BORDER_HEIGHT_DP;

        // Dynamic values.
        mSearchBarHeight = mSearchBarHeightPeeking;
    }

    /**
     * Gets the height of the Contextual Search Panel in dps for a given
     * |state|.
     *
     * @param state The state whose height will be calculated.
     * @return The height of the Contextual Search Panel in dps for a given
     *         |state|.
     */
    protected float getPanelHeightFromState(PanelState state) {
        float fullscreenHeight = getFullscreenHeight();
        float panelHeight = 0;

        if (state == PanelState.UNDEFINED) {
            panelHeight = 0;
        } else if (state == PanelState.CLOSED) {
            panelHeight = 0;
        } else if (state == PanelState.PEEKED) {
            panelHeight = mSearchBarHeightPeeking;
        } else if (state == PanelState.EXPANDED) {
            panelHeight = fullscreenHeight * EXPANDED_PANEL_HEIGHT_PERCENTAGE
                    + PANEL_SHADOW_HEIGHT_DP;
        } else if (state == PanelState.MAXIMIZED) {
            panelHeight = fullscreenHeight + PANEL_SHADOW_HEIGHT_DP;
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

    @Override
    protected void setPanelState(PanelState state, StateChangeReason reason) {
        super.setPanelState(state, reason);

        if (state == PanelState.CLOSED) {
            mIsShowing = false;
            destroyPromoView();
            destroyContextualSearchControl();
            onClose();
        } else if (state == PanelState.EXPANDED) {
            showPromoViewAtYPosition(getPromoYPx());
        }
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
     * @param height The Contextual Search Panel height.
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
     * @param height The Contextual Search Panel height.
     * @param endState The final state of transition being executed.
     * @param percentage The completion percentage of the transition.
     */
    private void updatePanelSize(float height, PanelState endState, float percentage) {
        mWidth = getFullscreenWidth();
        mHeight = height;
        mOffsetY = getContextualSearchPanelY();
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
     * Gets the state completion percentage, taking into consideration the
     * |height| of the Contextual Search Panel, and the initial and final
     * states. A completion of 0 means the Panel is in the initial state and a
     * completion of 1 means the Panel is in the final state.
     *
     * @param height The height of the Contextual Search Panel.
     * @param startState The initial state of the Panel.
     * @param endState The final state of the Panel.
     * @return The completion percentage.
     */
    private float getStateCompletion(float height, PanelState startState, PanelState endState) {
        float startSize = getPanelHeightFromState(startState);
        float endSize = getPanelHeightFromState(endState);
        float percentage = (height - startSize) / (endSize - startSize);
        return percentage;
    }

    /**
     * Updates the UI state for the closed to peeked transition (and vice
     * versa), according to a completion |percentage|.
     *
     * @param percentage The completion percentage.
     */
    private void updatePanelForCloseOrPeek(float percentage) {
        // Update the opt out promo.
        updatePromoVisibility(1.f);

        // Base page offset.
        mBasePageY = 0.f;

        // Base page brightness.
        mBasePageBrightness = BASE_PAGE_BRIGHTNESS_STATE_PEEKED;

        // Search Bar height.
        mSearchBarHeight = mSearchBarHeightPeeking;

        // Search Bar border.
        mIsSearchBarBorderVisible = false;

        // Search Bar text opacity.
        mSearchBarTextOpacity = 1.f;

        // Search provider icon opacity.
        mSearchProviderIconOpacity = SEARCH_PROVIDER_ICON_OPACITY_STATE_PEEKED;

        // Search icon opacity.
        mSearchIconOpacity = SEARCH_ICON_OPACITY_PEEKED;

        // Progress Bar.
        mProgressBarOpacity = 0.f;

        // Update the Search Bar Shadow.
        updateSearchBarShadow();
    }

    /**
     * Updates the UI state for the peeked to expanded transition (and vice
     * versa), according to a completion |percentage|.
     *
     * @param percentage The completion percentage.
     */
    private void updatePanelForExpansion(float percentage) {
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

        // Search Bar height.
        float searchBarHeight = Math.round(MathUtils.interpolate(
                mSearchBarHeightPeeking,
                mSearchBarHeightExpanded,
                percentage));
        mSearchBarHeight = searchBarHeight;

        // Search Bar text opacity.
        mSearchBarTextOpacity = 1.f;

        // Search Bar border.
        mIsSearchBarBorderVisible = true;
        mSearchBarBorderY = searchBarHeight - SEARCH_BAR_BORDER_HEIGHT_DP + 1;

        // Search provider icon opacity.
        mSearchProviderIconOpacity = SEARCH_PROVIDER_ICON_OPACITY_STATE_PEEKED;

        // Search icon opacity.
        mSearchIconOpacity = SEARCH_ICON_OPACITY_PEEKED;

        // Progress Bar.
        float peekedHeight = getPanelHeightFromState(PanelState.PEEKED);
        float threshold = PROGRESS_BAR_VISIBILITY_THRESHOLD_DP / mPxToDp;
        float diff = Math.min(mHeight - peekedHeight, threshold);
        // Fades the Progress Bar the closer it gets to the bottom of the
        // screen.
        float progressBarOpacity = MathUtils.interpolate(0.f, 1.f, diff / threshold);
        mProgressBarOpacity = progressBarOpacity;
        mProgressBarY = searchBarHeight - PROGRESS_BAR_HEIGHT_DP + 1;

        // Update the Search Bar Shadow.
        updateSearchBarShadow();
    }

    /**
     * Updates the UI state for the expanded to maximized transition (and vice
     * versa), according to a completion |percentage|.
     *
     * @param percentage The completion percentage.
     */
    private void updatePanelForMaximization(float percentage) {
        // Update the opt out promo.
        updatePromoVisibility(1.f - percentage);

        // Base page offset.
        mBasePageY = getBasePageTargetY();

        // Base page brightness.
        float brightness = MathUtils.interpolate(
                BASE_PAGE_BRIGHTNESS_STATE_EXPANDED,
                BASE_PAGE_BRIGHTNESS_STATE_MAXIMIZED,
                percentage);
        mBasePageBrightness = brightness;

        // Search Bar height.
        float searchBarHeight = Math.round(MathUtils.interpolate(
                mSearchBarHeightExpanded,
                mSearchBarHeightMaximized,
                percentage));
        mSearchBarHeight = searchBarHeight;

        // Search Bar border.
        mIsSearchBarBorderVisible = true;
        mSearchBarBorderY = searchBarHeight - SEARCH_BAR_BORDER_HEIGHT_DP + 1;

        // Search Bar text opacity.
        mSearchBarTextOpacity = 1.f;

        // Search provider icon opacity.
        float searchProviderIconOpacity = MathUtils.interpolate(
                SEARCH_PROVIDER_ICON_OPACITY_STATE_EXPANDED,
                SEARCH_PROVIDER_ICON_OPACITY_STATE_MAXIMIZED,
                percentage);
        mSearchProviderIconOpacity = searchProviderIconOpacity;

        // Search icon opacity.
        float searchIconOpacity = MathUtils.interpolate(
                SEARCH_ICON_OPACITY_EXPANDED,
                SEARCH_ICON_OPACITY_MAXIMIZED,
                percentage);
        mSearchIconOpacity = searchIconOpacity;

        // Progress Bar.
        mProgressBarOpacity = 1.f;
        mProgressBarY = searchBarHeight - PROGRESS_BAR_HEIGHT_DP + 1;

        // Update the Search Bar Shadow.
        updateSearchBarShadow();
    }

    /**
     * Updates the UI state for Opt Out Promo.
     *
     * @param percentage The visibility percentage of the Promo. A visibility of 0 means the
     * Promo is not visible. A visibility of 1 means the Promo is fully visible. And
     * visibility between 0 and 1 means the Promo is partially visible.
     */
    private void updatePromoVisibility(float percentage) {
        if (isPromoAvailable()) {
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
     * Updates the UI state for Search Bar Shadow.
     */
    public void updateSearchBarShadow() {
        float searchBarShadowHeightPx = 9.f / mPxToDp;
        if (mPromoVisible && mPromoHeightPx > 0.f) {
            mSearchBarShadowVisible = true;
            float threshold = 2 * searchBarShadowHeightPx;
            mSearchBarShadowOpacity = mPromoHeightPx > searchBarShadowHeightPx ? 1.f :
                MathUtils.interpolate(0.f, 1.f, mPromoHeightPx / threshold);
        } else {
            mSearchBarShadowVisible = false;
            mSearchBarShadowOpacity = 0.f;
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
    private void updateBasePageTargetY() {
        mBasePageTargetY = calculateBasePageTargetY(PanelState.EXPANDED);
    }

    /**
     * Calculates the target offset of the Base Page in order to keep the selection in view
     * after expanding the Panel to the given |expandedState|.
     *
     * @param expandedState
     * @return The target offset Y.
     */
    private float calculateBasePageTargetY(PanelState expandedState) {
        // Convert from px to dp.
        final float selectionY = mBasePageSelectionYPx * mPxToDp;

        // Calculate the exact height of the expanded Panel without taking into
        // consideration the height of the shadow (what is returned by the
        // getPanelFromHeight method). We need the measurement of the portion
        // of the Panel that occludes the page.
        final float expandedHeight = getPanelHeightFromState(expandedState)
                - PANEL_SHADOW_HEIGHT_DP;

        // Calculate the offset to center the selection on the available area.
        final float fullscreenHeight = getFullscreenHeight();
        final float availableHeight = fullscreenHeight - expandedHeight;
        float offset = -selectionY + availableHeight / 2;

        // Make sure offset is negative to prevent Base Page from moving down,
        // because there's nothing to render above the Page.
        offset = Math.min(offset, 0.f);
        // If visible, the Toolbar will be hidden. Therefore, we need to adjust
        // the offset to account for this difference.
        if (mIsToolbarShowing) offset -= TOOLBAR_HEIGHT_DP;
        // Make sure the offset is not greater than the expanded height, because
        // there's nothing to render below the Page.
        offset = Math.max(offset, -expandedHeight);

        return offset;
    }

    /**
     * @return The Y coordinate to apply to the Base Page in order to keep the selection
     *         in view when the Search Panel is in EXPANDED state.
     */
    public float getBasePageTargetY() {
        return mBasePageTargetY;
    }

    // ============================================================================================
    // Resource Loader
    // ============================================================================================

    private ViewGroup mContainerView;
    private DynamicResourceLoader mResourceLoader;

    /**
     * @param resourceLoader The {@link DynamicResourceLoader} to register and unregister the view.
     */
    public void setDynamicResourceLoader(DynamicResourceLoader resourceLoader) {
        mResourceLoader = resourceLoader;

        if (mControl != null) {
            mResourceLoader.registerResource(R.id.contextual_search_view,
                    mControl.getResourceAdapter());
        }

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
    // ContextualSearchControl
    // ============================================================================================

    // TODO(pedrosimonetti): rename this to something more generic (e.g. BottomBarTextView).

    private ContextualSearchControl mControl;

    /**
     * Inflates the Contextual Search control, if needed. The View will be set to INVISIBLE
     * after being inflated, because it won't actually be displayed on the screen (its
     * snapshot will be displayed instead).
     */
    protected ContextualSearchControl getContextualSearchControl() {
        assert mContainerView != null;

        if (mControl == null) {
            LayoutInflater.from(mContext).inflate(R.layout.contextual_search_view, mContainerView);
            mControl = (ContextualSearchControl)
                    mContainerView.findViewById(R.id.contextual_search_view);
            if (mResourceLoader != null) {
                mResourceLoader.registerResource(R.id.contextual_search_view,
                        mControl.getResourceAdapter());
            }
        }

        assert mControl != null;
        // TODO(pedrosimonetti): For now, we're still relying on a Android View
        // to render the text that appears in the Search Bar. The View will be
        // invisible and will not capture events. Consider rendering the text
        // in the Compositor and get rid of the View entirely.
        mControl.setVisibility(View.INVISIBLE);
        return mControl;
    }

    protected void destroyContextualSearchControl() {
        if (mControl != null) {
            mContainerView.removeView(mControl);
            mControl = null;
            if (mResourceLoader != null) {
                mResourceLoader.unregisterResource(R.id.contextual_search_view);
            }
        }
    }

    // ============================================================================================
    // Promo Host
    // ============================================================================================

    @Override
    public void onPromoPreferenceClick() {
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                setIsPromoActive(false);
                PreferencesLauncher.launchSettingsPage(mContext,
                        ContextualSearchPreferenceFragment.class.getName());
            }
        });
    }

    @Override
    public void onPromoButtonClick(boolean accepted) {
        if (accepted) {
            animatePromoAcceptance();
        } else {
            hidePromoView();
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
    public void createPromoView() {
        if (!isPromoAvailable()) return;

        assert mContainerView != null;

        if (mPromoView == null) {
            LayoutInflater.from(mContext).inflate(
                    R.layout.contextual_search_opt_out_promo, mContainerView);
            mPromoView = (ContextualSearchOptOutPromo)
                    mContainerView.findViewById(R.id.contextual_search_opt_out_promo);
            if (mResourceLoader != null) {
                mResourceLoader.registerResource(R.id.contextual_search_opt_out_promo,
                        mPromoView.getResourceAdapter());
            }

            mPromoView.setPromoHost(this);
            setPromoContentHeightPx(mPromoView.getHeightForGivenWidth(mContainerView.getWidth()));
        }

        assert mPromoView != null;
    }

    /**
     * Destroys the Search Promo View.
     */
    protected void destroyPromoView() {
        if (!isPromoAvailable()) return;

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
        if (mPromoView == null || !isPromoAvailable()) return;

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
                || !isPromoAvailable()) {
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
        updateSearchBarShadow();
    }
}
