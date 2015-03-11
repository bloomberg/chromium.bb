// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.os.Handler;

import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManagementDelegate;

/**
 * Controls the Contextual Search Panel.
 */
public class ContextualSearchPanel extends ContextualSearchPanelAnimation
        implements ContextualSearchPanelDelegate {

    /**
     * State of the Contextual Search Panel.
     */
    public static enum PanelState {
        UNDEFINED,
        CLOSED,
        PEEKED,
        PROMO,
        EXPANDED,
        MAXIMIZED;
    }

    /**
     * The reason for a change in the Contextual Search Panel's state.
     */
    public static enum StateChangeReason {
        UNKNOWN,
        RESET,
        BACK_PRESS,
        TEXT_SELECT_TAP,
        TEXT_SELECT_LONG_PRESS,
        INVALID_SELECTION,
        BASE_PAGE_TAP,
        BASE_PAGE_SCROLL,
        SEARCH_BAR_TAP,
        SERP_NAVIGATION,
        TAB_PROMOTION,
        CLICK,
        SWIPE,
        FLING,
        OPTIN,
        OPTOUT;
    }

    /**
     * The delay after which the hide progress will be hidden.
     */
    private static final long HIDE_PROGRESS_BAR_DELAY = 1000 / 60 * 4;

    /**
     * The initial height of the Contextual Search Panel.
     */
    private float mInitialPanelHeight;

    /**
     * Whether the Panel should be promoted to a new tab after being maximized.
     */
    private boolean mShouldPromoteToTabAfterMaximizing;

    /**
     * Whether a touch gesture has been detected.
     */
    private boolean mHasDetectedTouchGesture;

    /**
     * Whether the search content view has been touched.
     */
    private boolean mHasSearchContentViewBeenTouched;

    /**
     * The {@link ContextualSearchPanelHost} used to communicate with the supported layout.
     */
    private ContextualSearchPanelHost mSearchPanelHost;

    /**
     * The object for handling global Contextual Search management duties
     */
    private ContextualSearchManagementDelegate mManagementDelegate;

    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * @param context The current Android {@link Context}.
     * @param updateHost The {@link LayoutUpdateHost} used to request updates in the Layout.
     */
    public ContextualSearchPanel(Context context, LayoutUpdateHost updateHost) {
        super(context, updateHost);
    }

    // ============================================================================================
    // Layout Integration
    // ============================================================================================

    /**
     * Sets the {@ContextualSearchPanelHost} used to communicate with the supported layout.
     * @param host The {@ContextualSearchPanelHost}.
     */
    public void setHost(ContextualSearchPanelHost host) {
        mSearchPanelHost = host;
    }

    // ============================================================================================
    // Contextual Search Manager Integration
    // ============================================================================================

    /**
     * Sets the {@code ContextualSearchManagementDelegate} associated with this Layout.
     * @param delegate The {@code ContextualSearchManagementDelegate}.
     */
    public void setManagementDelegate(ContextualSearchManagementDelegate delegate) {
        mManagementDelegate = delegate;
    }

    /**
     * @return The {@code ContextualSearchManagementDelegate} associated with this Layout.
     */
    public ContextualSearchManagementDelegate getManagementDelegate() {
        return mManagementDelegate;
    }

    /**
     * Sets the visibility of the Search Content View.
     * @param isVisible True to make it visible.
     */
    public void setSearchContentViewVisibility(boolean isVisible) {
        if (mManagementDelegate != null) {
            mManagementDelegate.setSearchContentViewVisibility(isVisible);
        }
    }

    // ============================================================================================
    // Generic Event Handling
    // ============================================================================================

    /**
     * Handles the beginning of the swipe gesture.
     */
    public void handleSwipeStart() {
        if (animationIsRunning()) {
            cancelAnimation(this, Property.PANEL_HEIGHT);
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
        if (ty > 0 && getPanelState() == PanelState.MAXIMIZED) {
            // Resets the Search Content View scroll position when swiping the Panel down
            // after being maximized.
            mManagementDelegate.resetSearchContentViewScroll();
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
     * Handles the click gesture.
     *
     * @param time The timestamp of the gesture.
     * @param x The x coordinate of the gesture.
     * @param y The y coordinate of the gesture.
     */
    public void handleClick(long time, float x, float y) {
        mHasDetectedTouchGesture = true;
        if (isYCoordinateInsideBasePage(y)) {
            closePanel(StateChangeReason.BASE_PAGE_TAP, true);
        } else if (isYCoordinateInsideSearchBar(y)) {
            // TODO(pedrosimonetti): handle click in the close button here.
            if (isPeeking()) {
                if (mManagementDelegate.isRunningInCompatibilityMode()) {
                    mManagementDelegate.openResolvedSearchUrlInNewTab();
                } else {
                    // NOTE(pedrosimonetti): If the promo is active and getPromoContentHeight()
                    // returns -1 that means that the promo page hasn't finished loading, and
                    // therefore it wasn't possible to calculate the height of the promo contents.
                    // This will only happen if the user taps on a word that will trigger the
                    // promo, and then quickly taps on the peeking bar, before the promo page
                    // (which is local) finishes loading.
                    //
                    // TODO(pedrosimonetti): For now, we're simply ignoring the tap action in
                    // that case. Consider implementing a better approach, where the Panel
                    // would auto-expand once the height is calculated.
                    if (!getIsPromoActive() || getPromoContentHeight() != -1) {
                        expandPanel(StateChangeReason.SEARCH_BAR_TAP);
                    }
                }
            } else if (isExpanded()) {
                peekPanel(StateChangeReason.SEARCH_BAR_TAP);
            } else if (isMaximized()) {
                mManagementDelegate.promoteToTab(true);
            }
        }
    }

    // ============================================================================================
    // Gesture Event helpers
    // ============================================================================================

    /**
     * @param y The y coordinate in dp.
     * @return Whether the given |y| coordinate is inside the Search Bar area.
     */
    public boolean isYCoordinateInsideSearchBar(float y) {
        return !isYCoordinateInsideBasePage(y) && !isYCoordinateInsideSearchContentView(y);
    }

    /**
     * @param y The y coordinate in dp.
     * @return Whether the given |y| coordinate is inside the Search Content
     *         View area.
     */
    public boolean isYCoordinateInsideSearchContentView(float y) {
        return y > getSearchContentViewOffsetY();
    }

    /**
     * @return The vertical offset of the Search Content View in dp.
     */
    public float getSearchContentViewOffsetY() {
        return getOffsetY() + getSearchBarHeight();
    }

    /**
     * @param y The y coordinate in dp.
     * @return Whether the given |y| coordinate is inside the Base Page area.
     */
    private boolean isYCoordinateInsideBasePage(float y) {
        return y < getOffsetY();
    }

    /**
     * @return Whether the Panel is in its expanded state.
     */
    protected boolean isExpanded() {
        return doesPanelHeightMatchState(PanelState.EXPANDED);
    }

    /**
     * Acknowledges that there was a touch in the search content view, though no immediate action
     * needs to be taken.
     */
    public void onTouchSearchContentViewAck() {
        mHasSearchContentViewBeenTouched = true;
    }

    // ============================================================================================
    // Animation Handling
    // ============================================================================================

    @Override
    protected void onAnimationFinished() {
        super.onAnimationFinished();

        if (shouldHideContextualSearchLayout()) {
            if (mSearchPanelHost != null) {
                mSearchPanelHost.hideLayout(false);
            }
            if (getPanelState() == PanelState.CLOSED) {
                mManagementDelegate.dismissContextualSearchBar();
            }
        }

        if (mShouldPromoteToTabAfterMaximizing && getPanelState() == PanelState.MAXIMIZED) {
            mShouldPromoteToTabAfterMaximizing = false;
            mManagementDelegate.promoteToTab(false);
        }
    }

    /**
    * Whether the Contextual Search Layout should be hidden.
    *
    * @return Whether the Contextual Search Layout should be hidden.
    */
    private boolean shouldHideContextualSearchLayout() {
        final PanelState state = getPanelState();

        return (state == PanelState.PEEKED || state == PanelState.CLOSED)
                && getHeight() == getPanelHeightFromState(state);
    }

    // ============================================================================================
    // Panel Delegate
    // ============================================================================================

    @Override
    public boolean isShowing() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.isShowing();
    }

    @Override
    public boolean isPeeking() {
        return doesPanelHeightMatchState(PanelState.PEEKED);
    }

    @Override
    public void maximizePanelThenPromoteToTab(StateChangeReason reason) {
        mShouldPromoteToTabAfterMaximizing = true;
        maximizePanel(reason);
    }

    @Override
    public void maximizePanelThenPromoteToTab(StateChangeReason reason, long duration) {
        mShouldPromoteToTabAfterMaximizing = true;
        animatePanelToState(PanelState.MAXIMIZED, reason, duration);
    }

    @Override
    public void peekPanel(StateChangeReason reason) {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.peekPanel(reason);

        if (getPanelState() == PanelState.CLOSED || getPanelState() == PanelState.PEEKED) {
            mHasSearchContentViewBeenTouched = false;
        }
    }

    @Override
    public void closePanel(StateChangeReason reason, boolean animate) {
        // If the close action is animated, the Layout will be hidden when
        // the animation is finished, so we should only hide the Layout
        // here when not animating.
        if (!animate && mSearchPanelHost != null) {
            mSearchPanelHost.hideLayout(true);
        }
        mHasSearchContentViewBeenTouched = false;

        super.closePanel(reason, animate);
    }

    @Override
    public void updateBasePageSelectionYPx(float y) {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.updateBasePageSelectionYPx(y);
    }

    @Override
    public void setPromoContentHeight(float height) {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.setPromoContentHeight(height);
    }

    @Override
    public void setShouldHidePromoHeader(boolean shouldHidePromoHeader) {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.setShouldHidePromoHeader(shouldHidePromoHeader);
    }

    @Override
    public void animateAfterFirstRunSuccess() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.animateAfterFirstRunSuccess();
    }

    @Override
    public void onLoadStarted() {
        setProgressBarCompletion(0);
        setProgressBarVisible(true);
        requestUpdate();
    }

    @Override
    public void onLoadStopped() {
        // Hides the Progress Bar after a delay to make sure it is rendered for at least
        // a few frames, otherwise its completion won't be visually noticeable.
        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                setProgressBarVisible(false);
                requestUpdate();
            }
        }, HIDE_PROGRESS_BAR_DELAY);
    }

    @Override
    public void onLoadProgressChanged(int progress) {
        setProgressBarCompletion(progress);
        requestUpdate();
    }

    @Override
    public PanelState getPanelState() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.getPanelState();
    }

    @Override
    public void setDidSearchInvolvePromo() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.setDidSearchInvolvePromo();
    }

    @Override
    public void setWasSearchContentViewSeen() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.setWasSearchContentViewSeen();
    }

    @Override
    public void setIsPromoActive(boolean shown) {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.setIsPromoActive(shown);
    }

    @Override
    public boolean didTouchSearchContentView() {
        return mHasSearchContentViewBeenTouched;
    }
}
