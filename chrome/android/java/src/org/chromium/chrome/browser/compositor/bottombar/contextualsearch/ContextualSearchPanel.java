// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;

import org.chromium.base.ActivityState;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.PanelPriority;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.scene_layer.ContextualSearchSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManagementDelegate;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.ui.resources.ResourceManager;

/**
 * Controls the Contextual Search Panel.
 */
public class ContextualSearchPanel extends OverlayPanel {

    /**
     * The delay after which the hide progress will be hidden.
     */
    private static final long HIDE_PROGRESS_BAR_DELAY = 1000 / 60 * 4;

    /**
     * Whether the Panel should be promoted to a new tab after being maximized.
     */
    private boolean mShouldPromoteToTabAfterMaximizing;

    /**
     * Used for logging state changes.
     */
    private final ContextualSearchPanelMetrics mPanelMetrics;

    /**
     * The object for handling global Contextual Search management duties
     */
    private ContextualSearchManagementDelegate mManagementDelegate;

    /**
     * Whether the content view has been touched.
     */
    private boolean mHasContentBeenTouched;

    /**
     * The compositor layer used for drawing the panel.
     */
    private ContextualSearchSceneLayer mSceneLayer;

    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * @param context The current Android {@link Context}.
     * @param updateHost The {@link LayoutUpdateHost} used to request updates in the Layout.
     * @param panelManager The object managing the how different panels are shown.
     */
    public ContextualSearchPanel(Context context, LayoutUpdateHost updateHost,
                OverlayPanelManager panelManager) {
        super(context, updateHost, panelManager);
        mSceneLayer = createNewContextualSearchSceneLayer();
        mPanelMetrics = new ContextualSearchPanelMetrics();
    }

    @Override
    public OverlayPanelContent createNewOverlayPanelContent() {
        return new OverlayPanelContent(mManagementDelegate.getOverlayContentDelegate(),
                new PanelProgressObserver(), mActivity);
    }

    /**
     * Default loading animation for a panel.
     */
    public class PanelProgressObserver extends OverlayContentProgressObserver {

        @Override
        public void onProgressBarStarted() {
            setProgressBarCompletion(0);
            setProgressBarVisible(true);
            requestUpdate();
        }

        @Override
        public void onProgressBarUpdated(int progress) {
            setProgressBarCompletion(progress);
            requestUpdate();
        }

        @Override
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
    // Scene layer
    // ============================================================================================

    @Override
    public SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    @Override
    public void updateSceneLayer(ResourceManager resourceManager) {
        if (mSceneLayer == null) return;

        mSceneLayer.update(resourceManager, this,
                getSearchContextViewId(),
                getSearchTermViewId(),
                getPeekPromoControl(),
                getSearchBarContextOpacity(),
                getSearchBarTermOpacity(),
                getIconSpriteControl());
    }

    /**
     * Create a new scene layer for this panel. This should be overridden by tests as necessary.
     */
    protected ContextualSearchSceneLayer createNewContextualSearchSceneLayer() {
        return new ContextualSearchSceneLayer(mContext.getResources().getDisplayMetrics().density);
    }

    // ============================================================================================
    // Contextual Search Manager Integration
    // ============================================================================================

    /**
     * Sets the {@code ContextualSearchManagementDelegate} associated with this panel.
     * @param delegate The {@code ContextualSearchManagementDelegate}.
     */
    public void setManagementDelegate(ContextualSearchManagementDelegate delegate) {
        if (mManagementDelegate != delegate) {
            mManagementDelegate = delegate;
            if (delegate != null) {
                setChromeActivity(mManagementDelegate.getChromeActivity());
                initializeUiState();
            }
        }
    }

    /**
     * @return The {@code ContextualSearchManagementDelegate} associated with this Layout.
     */
    public ContextualSearchManagementDelegate getManagementDelegate() {
        return mManagementDelegate;
    }

    // ============================================================================================
    // Logging of panel state information.
    // ============================================================================================

    @Override
    public void setPanelState(PanelState toState, StateChangeReason reason) {
        // Store the previous state of the panel for when super changes it. 'super' should be the
        // first thing with significant logic that runs in this method which is why
        // onPanelStateChanged is not called here.
        PanelState fromState = getPanelState();
        super.setPanelState(toState, reason);

        mPanelMetrics.onPanelStateChanged(fromState, toState, reason);

        if (toState == PanelState.PEEKED
                && (fromState == PanelState.CLOSED || fromState == PanelState.UNDEFINED)) {
            // If the Peek Promo is visible, it should animate when the SearchBar peeks.
            if (getPeekPromoControl().isVisible()) {
                getPeekPromoControl().animateAppearance();
            }
            if (getIconSpriteControl().shouldAnimateAppearance()) {
                mPanelMetrics.setWasIconSpriteAnimated(true);
                getIconSpriteControl().animateApperance();
            } else {
                mPanelMetrics.setWasIconSpriteAnimated(false);
            }
        }

        if (fromState == PanelState.PEEKED
                && (toState == PanelState.EXPANDED || toState == PanelState.MAXIMIZED)) {
            // After opening the Panel to either expanded or maximized state,
            // the promo should disappear.
            getPeekPromoControl().hide();
        }
    }

    // ============================================================================================
    // Promo Host
    // ============================================================================================

    @Override
    public void onPromoPreferenceClick() {
        super.onPromoPreferenceClick();
    }

    @Override
    public void onPromoButtonClick(boolean accepted) {
        super.onPromoButtonClick(accepted);
    }

    // ============================================================================================
    // Contextual Search Manager Integration
    // ============================================================================================

    @Override
    public void setPreferenceState(boolean enabled) {
        if (mManagementDelegate != null) {
            PrefServiceBridge.getInstance().setContextualSearchState(enabled);
            setIsPromoActive(false);
        }
    }

    @Override
    protected void onClosed(StateChangeReason reason) {
        // Must be called before destroying Content because unseen visits should be removed from
        // history, and if the Content gets destroyed there won't be a ContentViewCore to do that.
        mManagementDelegate.onCloseContextualSearch(reason);

        super.onClosed(reason);
    }

    // ============================================================================================
    // Generic Event Handling
    // ============================================================================================

    @Override
    public void handleBarClick(long time, float x, float y) {
        super.handleBarClick(time, x, y);
        if (isExpanded() || isMaximized()) {
            if (isCoordinateInsideCloseButton(x)) {
                closePanel(StateChangeReason.CLOSE_BUTTON, true);
            } else if (!mActivity.isCustomTab()) {
                getManagementDelegate().promoteToTab();
            }
        }
    }

    @Override
    public boolean onInterceptBarClick() {
        return onInterceptOpeningPanel();
    }

    @Override
    public boolean onInterceptBarSwipe() {
        return onInterceptOpeningPanel();
    }

    /**
     * @return True if the event on the bar was intercepted.
     */
    private boolean onInterceptOpeningPanel() {
        if (mManagementDelegate.isRunningInCompatibilityMode()) {
            mManagementDelegate.openResolvedSearchUrlInNewTab();
            return true;
        }
        return false;
    }

    // ============================================================================================
    // Panel base methods
    // ============================================================================================

    @Override
    protected void destroyComponents() {
        super.destroyComponents();
        destroyPromoView();
        destroyPeekPromoControl();
        destroySearchBarControl();
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        super.onActivityStateChange(activity, newState);
        if (newState == ActivityState.PAUSED) {
            mManagementDelegate.logCurrentState();
        }
    }

    @Override
    public PanelPriority getPriority() {
        return PanelPriority.HIGH;
    }

    @Override
    public boolean canBeSuppressed() {
        // The selected text on the page is lost when the panel is closed, thus, this panel cannot
        // be restored if it is suppressed.
        return false;
    }

    @Override
    public boolean supportsContextualSearchLayout() {
        return mManagementDelegate != null && !mManagementDelegate.isRunningInCompatibilityMode();
    }

    @Override
    public void notifyBarTouched(float x) {
        // Directly notify the content that it was touched since the close button is not displayed
        // when Contextual Search is peeking.
        getOverlayPanelContent().notifyBarTouched();
    }

    // ============================================================================================
    // Animation Handling
    // ============================================================================================

    @Override
    protected void onAnimationFinished() {
        super.onAnimationFinished();

        if (mShouldPromoteToTabAfterMaximizing && getPanelState() == PanelState.MAXIMIZED) {
            mShouldPromoteToTabAfterMaximizing = false;
            mManagementDelegate.promoteToTab();
        }
    }

    @Override
    public void setProperty(Property prop, float value) {
        super.setProperty(prop, value);
        if (prop == Property.BOTTOM_BAR_TEXT_VISIBILITY) {
            updateSearchBarTextOpacity(value);
        }
    }

    // ============================================================================================
    // Contextual Search Panel API
    // ============================================================================================

    /**
     * Notify the panel that the ContentViewCore was seen.
     */
    public void setWasSearchContentViewSeen() {
        mPanelMetrics.setWasSearchContentViewSeen();
    }

    /**
     * @param isActive Whether the promo is active.
     */
    public void setIsPromoActive(boolean isActive) {
        setPromoVisibility(isActive);
        mPanelMetrics.setIsPromoActive(isActive);
    }

    /**
     * Shows the peek promo.
     */
    public void showPeekPromo() {
        getPeekPromoControl().show();
    }

    /**
     * @return Whether the Peek Promo is visible.
     */
    @VisibleForTesting
    public boolean isPeekPromoVisible() {
        return getPeekPromoControl().isVisible();
    }

    /**
     * Called when the SERP finishes loading, this records the duration of loading the SERP from
     * the time the panel was opened until the present.
     * @param wasPrefetch Whether the request was prefetch-enabled.
     */
    public void onSearchResultsLoaded(boolean wasPrefetch) {
        mPanelMetrics.onSearchResultsLoaded(wasPrefetch);
    }

    /**
     * Maximizes the Contextual Search Panel, then promotes it to a regular Tab.
     * @param reason The {@code StateChangeReason} behind the maximization and promotion to tab.
     */
    public void maximizePanelThenPromoteToTab(StateChangeReason reason) {
        mShouldPromoteToTabAfterMaximizing = true;
        maximizePanel(reason);
    }

    /**
     * Maximizes the Contextual Search Panel, then promotes it to a regular Tab.
     * @param reason The {@code StateChangeReason} behind the maximization and promotion to tab.
     * @param duration The animation duration in milliseconds.
     */
    public void maximizePanelThenPromoteToTab(StateChangeReason reason, long duration) {
        mShouldPromoteToTabAfterMaximizing = true;
        animatePanelToState(PanelState.MAXIMIZED, reason, duration);
    }

    @Override
    public void peekPanel(StateChangeReason reason) {
        super.peekPanel(reason);

        if (getPanelState() == PanelState.CLOSED || getPanelState() == PanelState.PEEKED) {
            mHasContentBeenTouched = false;
        }
    }

    @Override
    public void closePanel(StateChangeReason reason, boolean animate) {
        super.closePanel(reason, animate);
        mHasContentBeenTouched = false;
    }

    @Override
    public void updateBasePageSelectionYPx(float y) {
        // NOTE(pedrosimonetti): exposing superclass method.
        super.updateBasePageSelectionYPx(y);
    }

    @Override
    public PanelState getPanelState() {
        // NOTE(pedrosimonetti): exposing superclass method to the interface.
        return super.getPanelState();
    }

    /**
     * Gets whether a touch on the content view has been done yet or not.
     */
    public boolean didTouchContent() {
        return mHasContentBeenTouched;
    }

    /**
     * @return {@code true} Whether the close animation should run when the the panel is closed
     *                      due the panel being promoted to a tab.
     */
    public boolean shouldAnimatePanelCloseOnPromoteToTab() {
        // TODO(pedrosimonetti): This is not currently used.
        return mActivity.isCustomTab();
    }

    /**
     * Shows the search term in the SearchBar. This should be called when the search term is set
     * without search term resolution.
     * @param searchTerm The string that represents the search term.
     */
    public void displaySearchTerm(String searchTerm) {
        cancelSearchTermResolutionAnimation();
        getSearchBarControl().setSearchTerm(searchTerm);
        resetSearchBarTermOpacity();
    }

    /**
     * Shows the search context in the SearchBar.
     * @param selection The portion of the context that represents the user's selection.
     * @param end The portion of the context from the selection to its end.
     */
    public void displaySearchContext(String selection, String end) {
        cancelSearchTermResolutionAnimation();
        getSearchBarControl().setSearchContext(selection, end);
        resetSearchBarContextOpacity();
    }

    /**
     * Handles showing the resolved search term in the SearchBar.
     * @param searchTerm The string that represents the search term.
     */
    public void onSearchTermResolutionResponse(String searchTerm) {
        getSearchBarControl().setSearchTerm(searchTerm);
        animateSearchTermResolution();
    }

    /**
     * Sets that the contextual search involved the promo.
     */
    public void setDidSearchInvolvePromo() {
        mPanelMetrics.setDidSearchInvolvePromo();
    }

    // ============================================================================================
    // Panel Rendering
    // ============================================================================================

    // TODO(pedrosimonetti): generalize the dispatching of panel updates.

    @Override
    protected void updatePanelForCloseOrPeek(float percentage) {
        super.updatePanelForCloseOrPeek(percentage);

        getPeekPromoControl().onUpdateFromCloseToPeek(percentage);
    }

    @Override
    protected void updatePanelForExpansion(float percentage) {
        super.updatePanelForExpansion(percentage);

        getPeekPromoControl().onUpdateFromPeekToExpand(percentage);
    }

    @Override
    protected void updatePanelForMaximization(float percentage) {
        super.updatePanelForMaximization(percentage);

        getPeekPromoControl().onUpdateFromExpandToMaximize(percentage);
    }

    @Override
    protected void updatePanelForOrientationChange() {
        // TODO(pedrosimonetti): find a better way of resizing the promo upon rotation.
        // Destroys the Promo view so it can be properly resized. Once the Promo starts
        // using the ViewResourceInflater, we could probably just call invalidate.
        destroyPromoView();

        super.updatePanelForOrientationChange();
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

    /**
     * Updates the UI state for the SearchBar text. The search context view will fade out
     * while the search term fades in.
     *
     * @param percentage The visibility percentage of the search term view.
     */
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
    // Peek Promo
    // ============================================================================================

    private ContextualSearchPeekPromoControl mPeekPromoControl;

    /**
     * Creates the ContextualSearchPeekPromoControl, if needed.
     */
    public ContextualSearchPeekPromoControl getPeekPromoControl() {
        assert mContainerView != null;
        assert mResourceLoader != null;

        if (mPeekPromoControl == null) {
            mPeekPromoControl =
                    new ContextualSearchPeekPromoControl(this, mContext, mContainerView,
                            mResourceLoader);
        }

        return mPeekPromoControl;
    }

    /**
     * Destroys the ContextualSearchPeekPromoControl.
     */
    private void destroyPeekPromoControl() {
        if (mPeekPromoControl != null) {
            mPeekPromoControl.destroy();
            mPeekPromoControl = null;
        }
    }

    @Override
    protected float getPeekPromoHeightPeekingPx() {
        return getPeekPromoControl().getHeightPeekingPx();
    }

    @Override
    protected float getPeekPromoHeight() {
        return getPeekPromoControl().getHeightPx();
    }

    // ============================================================================================
    // Search Provider Icon Sprite
    // ============================================================================================

    private ContextualSearchIconSpriteControl mIconSpriteControl;

    /**
     * @return The {@link ContextualSearchIconSpriteControl} for the panel.
     */
    public ContextualSearchIconSpriteControl getIconSpriteControl() {
        if (mIconSpriteControl == null) {
            mIconSpriteControl = new ContextualSearchIconSpriteControl(this, mContext);
        }
        return mIconSpriteControl;
    }

    /**
     * @param shouldAnimateIconSprite Whether the search provider icon sprite should be animated.
     * @param isAnimationDisabledByTrial Whether animating the search provider icon is disabled by a
     *                                   field trial.
     */
    public void setShouldAnimateIconSprite(boolean shouldAnimateIconSprite,
            boolean isAnimationDisabledByTrial) {
        getIconSpriteControl().setShouldAnimateAppearance(shouldAnimateIconSprite,
                isAnimationDisabledByTrial);
    }

    // ============================================================================================
    // Promo
    // ============================================================================================

    // TODO(pedrosimonetti): refactor the rest of the promo code into its own Control.

    /**
     * Whether the Promo is visible.
     */
    private boolean mIsPromoVisible;


    @Override
    protected boolean isPromoVisible() {
        return mIsPromoVisible;
    }

    @Override
    protected void onPromoAcceptanceAnimationFinished() {
        // NOTE(pedrosimonetti): We should only set the preference to true after the acceptance
        // animation finishes, because setting the preference will make the user leave the
        // undecided state, and that will, in turn, turn the promo off.
        setPreferenceState(true);
    }

    /**
     * @param isVisible Whether the Promo should be visible.
     */
    private void setPromoVisibility(boolean isVisible) {
        mIsPromoVisible = isVisible;

        if (mIsPromoVisible) {
            createPromoView();
        }
    }

    // ============================================================================================
    // Panel Content
    // ============================================================================================

    @Override
    public void onTouchSearchContentViewAck() {
        mHasContentBeenTouched = true;
    }

    /**
     * Destroy the current content in the panel.
     * NOTE(mdjones): This should not be exposed. The only use is in ContextualSearchManager for a
     * bug related to loading new panel content.
     */
    public void destroyContent() {
        super.destroyOverlayPanelContent();
    }
}
