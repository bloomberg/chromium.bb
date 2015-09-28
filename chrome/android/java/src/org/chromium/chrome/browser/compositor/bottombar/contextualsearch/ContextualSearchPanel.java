// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.os.Handler;
import android.view.View.MeasureSpec;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.ContentViewCore;

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
        CLOSE_BUTTON;
    }

    // The animation duration of a URL being promoted to a tab when triggered by an
    // intercept navigation. This is faster than the standard tab promotion animation
    // so that it completes before the navigation.
    private static final long INTERCEPT_NAVIGATION_PROMOTION_ANIMATION_DURATION_MS = 40;

    /**
     * The extra dp added around the close button touch target.
     */
    private static final int CLOSE_BUTTON_TOUCH_SLOP_DP = 5;

    /**
     * The activity this panel is in.
     */
    private ChromeActivity mActivity;

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
     * Container for content the panel will show.
     */
    private OverlayPanelContent mOverlayPanelContent;

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

    /**
     * Create a new OverlayPanelContent object. This can be overridden for tests.
     */
    public OverlayPanelContent createNewOverlayPanelContent() {
        OverlayPanelContent overlayPanelContent = new OverlayPanelContent(
                getManagementDelegate().getOverlayContentDelegate(), new PanelProgressObserver(),
                mActivity);

        // Adds a ContentViewClient to override the default fullscreen size.
        if (!isFullscreenSizePanel()) {
            overlayPanelContent.setContentViewClient(new ContentViewClient() {
                @Override
                public int getDesiredWidthMeasureSpec() {
                    return MeasureSpec.makeMeasureSpec(
                            getSearchContentViewWidthPx(),
                            MeasureSpec.EXACTLY);
                }

                @Override
                public int getDesiredHeightMeasureSpec() {
                    return MeasureSpec.makeMeasureSpec(
                            getSearchContentViewHeightPx(),
                            MeasureSpec.EXACTLY);
                }
            });
        }

        return overlayPanelContent;
    }

    /**
     * Default loading animation for a panel.
     */
    public class PanelProgressObserver extends OverlayContentProgressObserver {

        public void onProgressBarStarted() {
            setProgressBarCompletion(0);
            setProgressBarVisible(true);
            requestUpdate();
        }

        public void onProgressBarUpdated(int progress) {
            setProgressBarCompletion(progress);
            requestUpdate();
        }

        public void onProgressBarFinished() {
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

    @Override
    public void setPreferenceState(boolean enabled) {
        if (getManagementDelegate() != null) {
            getManagementDelegate().setPreferenceState(enabled);
        }
    }

    @Override
    protected boolean isPromoAvailable() {
        return getManagementDelegate() != null && getManagementDelegate().isPromoAvailable();
    }

    @Override
    public void onPromoButtonClick(boolean accepted) {
        super.onPromoButtonClick(accepted);
        getManagementDelegate().logPromoOutcome();
        setIsPromoActive(false);
    }

    @Override
    protected void onClose(StateChangeReason reason) {
        destroySearchBarControl();
        if (mOverlayPanelContent != null) {
            mOverlayPanelContent.destroyContentView();
        }
        getManagementDelegate().onCloseContextualSearch(reason);
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
        if (ty > 0 && getPanelState() == PanelState.MAXIMIZED) {
            // Resets the Search Content View scroll position when swiping the Panel down
            // after being maximized.
            mOverlayPanelContent.resetContentViewScroll();
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
        if (isCoordinateInsideBasePage(x, y)) {
            closePanel(StateChangeReason.BASE_PAGE_TAP, true);
        } else if (isCoordinateInsideSearchBar(x, y)) {
            if (isPeeking()) {
                if (getManagementDelegate().isRunningInCompatibilityMode()) {
                    getManagementDelegate().openResolvedSearchUrlInNewTab();
                } else {
                    if (isFullscreenSizePanel()) {
                        expandPanel(StateChangeReason.SEARCH_BAR_TAP);
                    } else {
                        maximizePanel(StateChangeReason.SEARCH_BAR_TAP);
                    }
                }
            } else if (isExpanded() || isMaximized()) {
                if (isCoordinateInsideCloseButton(x, y)) {
                    closePanel(StateChangeReason.CLOSE_BUTTON, true);
                } else if (mSearchPanelFeatures.isSearchTermRefiningAvailable()) {
                    getManagementDelegate().promoteToTab();
                }
            }
        }
    }

    // ============================================================================================
    // Gesture Event helpers
    // ============================================================================================

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the Search Panel area.
     */
    private boolean isCoordinateInsideSearchPanel(float x, float y) {
        return y >= getOffsetY() && y <= (getOffsetY() + getHeight())
                &&  x >= getOffsetX() && x <= (getOffsetX() + getWidth());
    }

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the Base Page area.
     */
    private boolean isCoordinateInsideBasePage(float x, float y) {
        return !isCoordinateInsideSearchPanel(x, y);
    }

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the Search Bar area.
     */
    public boolean isCoordinateInsideSearchBar(float x, float y) {
        return isCoordinateInsideSearchPanel(x, y)
                && y >= getOffsetY() && y <= (getOffsetY() + getSearchBarHeight());
    }

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the Search Content View area.
     */
    public boolean isCoordinateInsideSearchContentView(float x, float y) {
        return isCoordinateInsideSearchPanel(x, y)
                && y > getSearchContentViewOffsetY();
    }

    /**
     * @return The horizontal offset of the Search Content View in dp.
     */
    public float getSearchContentViewOffsetX() {
        return getOffsetX();
    }

    /**
     * @return The vertical offset of the Search Content View in dp.
     */
    public float getSearchContentViewOffsetY() {
        return getOffsetY() + getSearchBarHeight() + getPromoHeight();
    }

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given |x| |y| coordinate is inside the close button.
     */
    private boolean isCoordinateInsideCloseButton(float x, float y) {
        boolean isInY = y >= (getCloseIconY() - CLOSE_BUTTON_TOUCH_SLOP_DP)
                && y <= (getCloseIconY() + getCloseIconDimension() + CLOSE_BUTTON_TOUCH_SLOP_DP);
        boolean isInX = x >= (getCloseIconX() - CLOSE_BUTTON_TOUCH_SLOP_DP)
                && x <= (getCloseIconX() + getCloseIconDimension() + CLOSE_BUTTON_TOUCH_SLOP_DP);
        return isInY && isInX;
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
        }

        if (mShouldPromoteToTabAfterMaximizing && getPanelState() == PanelState.MAXIMIZED) {
            mShouldPromoteToTabAfterMaximizing = false;
            getManagementDelegate().promoteToTab();
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
    // Utilities
    // ============================================================================================

    /**
     * @return The vertical scroll position of the content.
     */
    public float getSearchContentViewVerticalScroll() {
        return mOverlayPanelContent.getContentViewVerticalScroll();
    }

    /**
     * @return A new OverlayPanelContent if the instance was null or the existing one.
     */
    public OverlayPanelContent getOverlayPanelContent() {
        // Only create the content when necessary
        if (mOverlayPanelContent == null) {
            mOverlayPanelContent = createNewOverlayPanelContent();
        }
        return mOverlayPanelContent;
    }

    // ============================================================================================
    // Panel Delegate
    // ============================================================================================

    @Override
    public boolean isFullscreenSizePanel() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.isFullscreenSizePanel();
    }

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
    public int getMaximumWidthPx() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.getMaximumWidthPx();
    }

    @Override
    public int getMaximumHeightPx() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.getMaximumHeightPx();
    }

    @Override
    public int getSearchContentViewWidthPx() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.getSearchContentViewWidthPx();
    }

    @Override
    public int getSearchContentViewHeightPx() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.getSearchContentViewHeightPx();
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

    @Override
    public void onSearchResultsLoaded(boolean wasPrefetch) {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        super.onSearchResultsLoaded(wasPrefetch);
    }

    @Override
    public boolean shouldAnimatePanelCloseOnPromoteToTab() {
        return mSearchPanelFeatures.shouldAnimatePanelCloseOnPromoteToTab();
    }

    @Override
    public void displaySearchTerm(String searchTerm) {
        cancelSearchTermResolutionAnimation();
        getSearchBarControl().setSearchTerm(searchTerm);
        resetSearchBarTermOpacity();
    }

    @Override
    public void displaySearchContext(String selection, String start, String end) {
        cancelSearchTermResolutionAnimation();
        getSearchBarControl().setSearchContext(selection, start, end);
        resetSearchBarContextOpacity();
    }

    @Override
    public void onSearchTermResolutionResponse(String searchTerm) {
        getSearchBarControl().setSearchTerm(searchTerm);
        animateSearchTermResolution();
    }

    @Override
    public boolean isContentViewShowing() {
        return mOverlayPanelContent != null && mOverlayPanelContent.isContentViewShowing();
    }

    @Override
    public void setChromeActivity(ChromeActivity activity) {
        mActivity = activity;
    }

    @Override
    public void loadUrlInPanel(String url) {
        getOverlayPanelContent().loadUrl(url);
    }

    @Override
    public boolean didLoadAnyUrl() {
        return mOverlayPanelContent != null && mOverlayPanelContent.didLoadAnyUrl();
    }

    @Override
    public void updateTopControlState() {
        if (mOverlayPanelContent == null) return;

        if (isFullscreenSizePanel()) {
            // Consider the ContentView height to be fullscreen, and inform the system that
            // the Toolbar is always visible (from the Compositor's perspective), even though
            // the Toolbar and Base Page might be offset outside the screen. This means the
            // renderer will consider the ContentView height to be the fullscreen height
            // minus the Toolbar height.
            //
            // This is necessary to fix the bugs: crbug.com/510205 and crbug.com/510206
            mOverlayPanelContent.updateTopControlsState(false, true, false);
        } else {
            mOverlayPanelContent.updateTopControlsState(true, false, false);
        }
    }

    // ============================================================================================
    // ContextualSearchBarControl
    // ============================================================================================

    private ContextualSearchBarControl mContextualSearchBarControl;
    private float mSearchBarContextOpacity = 1.f;
    private float mSearchBarTermOpacity = 1.f;

    /**
     * @return The opacity of the SearchBar's search context.
     */
    public float getSearchBarContextOpacity() {
        return mSearchBarContextOpacity;
    }

    /**
     * @return The opacity of the SearchBar's search term.
     */
    public float getSearchBarTermOpacity() {
        return mSearchBarTermOpacity;
    }

    /**
     * @return The Id of the Search Context View.
     */
    public int getSearchContextViewId() {
        return getSearchBarControl().getSearchContextViewId();
    }

    /**
     * @return The Id of the Search Term View.
     */
    public int getSearchTermViewId() {
        return getSearchBarControl().getSearchTermViewId();
    }

    /**
     * Creates the ContextualSearchBarControl, if needed. The Views are set to INVISIBLE, because
     * they won't actually be displayed on the screen (their snapshots will be displayed instead).
     */
    protected ContextualSearchBarControl getSearchBarControl() {
        assert mContainerView != null;
        assert mResourceLoader != null;

        if (mContextualSearchBarControl == null) {
            mContextualSearchBarControl =
                    new ContextualSearchBarControl(this, mContext, mContainerView, mResourceLoader);
        }

        assert mContextualSearchBarControl != null;
        return mContextualSearchBarControl;
    }

    /**
     * Destroys the ContextualSearchBarControl.
     */
    protected void destroySearchBarControl() {
        if (mContextualSearchBarControl != null) {
            mContextualSearchBarControl.destroy();
            mContextualSearchBarControl = null;
        }
    }

    @Override
    protected void updateSearchBarTextOpacity(float percentage) {
        // The search context will start fading out before the search term starts fading in.
        // They will both be partially visible for overlapPercentage of the animation duration.
        float overlapPercentage = .75f;
        float fadingOutPercentage = Math.max(1 - (percentage / overlapPercentage), 0.f);
        float fadingInPercentage =
                Math.max(percentage - (1 - overlapPercentage), 0.f) / overlapPercentage;

        mSearchBarContextOpacity = fadingOutPercentage;
        mSearchBarTermOpacity = fadingInPercentage;
    }

    /**
     * Resets the SearchBar text opacity when a new search context is set. The search
     * context is made visible and the search term invisible.
     */
    private void resetSearchBarContextOpacity() {
        mSearchBarContextOpacity = 1.f;
        mSearchBarTermOpacity = 0.f;
    }

    /**
     * Resets the SearchBar text opacity when a new search term is set. The search
     * term is made visible and the search context invisible.
     */
    private void resetSearchBarTermOpacity() {
        mSearchBarContextOpacity = 0.f;
        mSearchBarTermOpacity = 1.f;
    }

    // ============================================================================================
    // Panel Content
    // ============================================================================================

    // TODO(pedrosimonetti): move content code to its own section.

    @Override
    public ContentViewCore getContentViewCore() {
        // Expose OverlayPanelContent method.
        return mOverlayPanelContent != null ? mOverlayPanelContent.getContentViewCore() : null;
    }

    @Override
    public void removeLastHistoryEntry(String historyUrl, long urlTimeMs) {
        if (mOverlayPanelContent == null) return;
        // Expose OverlayPanelContent method.
        mOverlayPanelContent.removeLastHistoryEntry(historyUrl, urlTimeMs);
    }

    @Override
    public void setSearchContentViewVisibility(boolean isVisible) {
        getOverlayPanelContent().setVisibility(isVisible);
    }

    /**
     * Destroy the native components associated with this panel's content.
     */
    public void destroy() {
        // It is possible that an OverlayPanelContent was never created for this panel.
        if (mOverlayPanelContent != null) {
            mOverlayPanelContent.destroy();
        }
    }

    @Override
    @VisibleForTesting
    public void setOverlayPanelContent(OverlayPanelContent panelContent) {
        mOverlayPanelContent = panelContent;
    }
}
