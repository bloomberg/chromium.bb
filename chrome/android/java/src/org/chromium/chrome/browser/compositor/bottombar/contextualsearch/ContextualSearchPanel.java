// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.ActivityState;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
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
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.privacy.ContextualSearchPreferenceFragment;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.ResourceManager;

/**
 * Controls the Contextual Search Panel.
 */
public class ContextualSearchPanel extends OverlayPanel
        implements ContextualSearchOptOutPromo.ContextualSearchPromoHost {

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
                getSearchBarControl(),
                getPeekPromoControl(),
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
            }
        }
    }

    // ============================================================================================
    // Panel State
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

        if (toState == PanelState.EXPANDED) {
            showPromoViewAtYPosition(getPromoYPx());
        }

        if (fromState == PanelState.PEEKED
                && (toState == PanelState.EXPANDED || toState == PanelState.MAXIMIZED)) {
            // After opening the Panel to either expanded or maximized state,
            // the promo should disappear.
            getPeekPromoControl().hide();
        }
    }

    @Override
    protected boolean isSupportedState(PanelState state) {
        return canDisplayContentInPanel() || state != PanelState.MAXIMIZED;
    }

    @Override
    protected float getExpandedHeight() {
        if (canDisplayContentInPanel()) {
            return super.getExpandedHeight();
        } else {
            return getBarHeightPeeking() + mPromoContentHeightPx * mPxToDp;
        }
    }

    // ============================================================================================
    // Contextual Search Manager Integration
    // ============================================================================================

    /**
     * @param enabled Whether the preference should be enabled.
     */
    public void setPreferenceState(boolean enabled) {
        if (mManagementDelegate != null) {
            PrefServiceBridge.getInstance().setContextualSearchState(enabled);
            setIsPromoActive(false, false);
        }
    }

    @Override
    protected void onClosed(StateChangeReason reason) {
        // Must be called before destroying Content because unseen visits should be removed from
        // history, and if the Content gets destroyed there won't be a ContentViewCore to do that.
        mManagementDelegate.onCloseContextualSearch(reason);

        setProgressBarCompletion(0);
        setProgressBarVisible(false);

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
            } else if (!mActivity.isCustomTab() && canDisplayContentInPanel()) {
                mManagementDelegate.promoteToTab();
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
        if (canDisplayContentInPanel()) {
            getOverlayPanelContent().showContent();
        }
    }

    // ============================================================================================
    // Animation Handling
    // ============================================================================================

    @Override
    protected void onAnimationFinished() {
        super.onAnimationFinished();

        if (mIsAnimatingPromoAcceptance) {
            mIsAnimatingPromoAcceptance = false;
            onPromoAcceptanceAnimationFinished();
        }

        if (mShouldPromoteToTabAfterMaximizing && getPanelState() == PanelState.MAXIMIZED) {
            mShouldPromoteToTabAfterMaximizing = false;
            mManagementDelegate.promoteToTab();
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
    public void setIsPromoActive(boolean isActive, boolean isMandatory) {
        mIsPromoMandatory = isMandatory;
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
     * Called after the panel has navigated to prefetched Search Results.
     * If the user has the panel open then they will see the prefetched result starting to load.
     * Currently this just logs the time between the start of the search until the results start to
     * render in the Panel.
     * @param didResolve Whether the search required the Search Term to be resolved.
     */
    public void onPanelNavigatedToPrefetchedSearch(boolean didResolve) {
        mPanelMetrics.onPanelNavigatedToPrefetchedSearch(didResolve);
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
     * Shows the search term in the SearchBar. This should be called when the search term is set
     * without search term resolution.
     * @param searchTerm The string that represents the search term.
     */
    public void displaySearchTerm(String searchTerm) {
        getSearchBarControl().setSearchTerm(searchTerm);
        mPanelMetrics.onSearchRequestStarted();
    }

    /**
     * Shows the search context in the SearchBar.
     * @param selection The portion of the context that represents the user's selection.
     * @param end The portion of the context from the selection to its end.
     */
    public void displaySearchContext(String selection, String end) {
        getSearchBarControl().setSearchContext(selection, end);
        mPanelMetrics.onSearchRequestStarted();
    }

    /**
     * Handles showing the resolved search term in the SearchBar.
     * @param searchTerm The string that represents the search term.
     */
    public void onSearchTermResolved(String searchTerm) {
        mPanelMetrics.onSearchTermResolved();
        getSearchBarControl().setSearchTerm(searchTerm);
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

        // Update the opt out promo.
        updatePromoVisibility(1.f);

        getPeekPromoControl().onUpdateFromCloseToPeek(percentage);
    }

    @Override
    protected void updatePanelForExpansion(float percentage) {
        super.updatePanelForExpansion(percentage);

        // Update the opt out promo.
        updatePromoVisibility(1.f);

        getPeekPromoControl().onUpdateFromPeekToExpand(percentage);
    }

    @Override
    protected void updatePanelForMaximization(float percentage) {
        super.updatePanelForMaximization(percentage);

        // Update the opt out promo.
        updatePromoVisibility(1.f - percentage);

        getPeekPromoControl().onUpdateFromExpandToMaximize(percentage);
    }

    @Override
    protected void updatePanelForOrientationChange() {
        if (isPromoVisible()) {
            updatePromoLayout();
        }

        // NOTE(pedrosimonetti): We cannot tell where the selection will be after the
        // orientation change, so we are setting the selection position to zero, which
        // means the base page will be positioned in its original state and we won't
        // try to keep the selection in view.
        updateBasePageSelectionYPx(0.f);
        updateBasePageTargetY();

        super.updatePanelForOrientationChange();
    }

    // ============================================================================================
    // Selection position
    // ============================================================================================

    /** The approximate Y coordinate of the selection in pixels. */
    private float mBasePageSelectionYPx = -1.f;

    /**
     * Updates the coordinate of the existing selection.
     * @param y The y coordinate of the selection in pixels.
     */
    public void updateBasePageSelectionYPx(float y) {
        mBasePageSelectionYPx = y;
    }

    @Override
    protected float calculateBasePageDesiredOffset() {
        float offset = 0.f;
        if (mBasePageSelectionYPx > 0.f) {
            // Convert from px to dp.
            final float selectionY = mBasePageSelectionYPx * mPxToDp;

            // Calculate the offset to center the selection on the available area.
            final float availableHeight = getTabHeight() - getExpandedHeight();
            offset = -selectionY + availableHeight / 2;
        }
        return offset;
    }

    // ============================================================================================
    // ContextualSearchBarControl
    // ============================================================================================

    private ContextualSearchBarControl mSearchBarControl;

    /**
     * Creates the ContextualSearchBarControl, if needed. The Views are set to INVISIBLE, because
     * they won't actually be displayed on the screen (their snapshots will be displayed instead).
     */
    protected ContextualSearchBarControl getSearchBarControl() {
        assert mContainerView != null;
        assert mResourceLoader != null;

        if (mSearchBarControl == null) {
            mSearchBarControl =
                    new ContextualSearchBarControl(this, mContext, mContainerView, mResourceLoader);
        }

        return mSearchBarControl;
    }

    /**
     * Destroys the ContextualSearchBarControl.
     */
    protected void destroySearchBarControl() {
        if (mSearchBarControl != null) {
            mSearchBarControl.destroy();
            mSearchBarControl = null;
        }
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
    // Peek Promo
    // ============================================================================================

    private ContextualSearchPeekPromoControl mPeekPromoControl;

    /**
     * @return The height of the Peek Promo in the peeked state, in pixels.
     */
    protected float getPeekPromoHeightPeekingPx() {
        return getPeekPromoControl().getHeightPeekingPx();
    }

    /**
     * @return The height of the Peek Promo, in pixels.
     */
    protected float getPeekPromoHeight() {
        return getPeekPromoControl().getHeightPx();
    }

    /**
     * Creates the ContextualSearchPeekPromoControl, if needed.
     */
    private ContextualSearchPeekPromoControl getPeekPromoControl() {
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

    // ============================================================================================
    // Promo
    // ============================================================================================

    // TODO(pedrosimonetti): refactor the rest of the promo code into its own Control.

    /**
     * Whether the Promo is visible.
     */
    private boolean mIsPromoVisible;

    /**
     * Whether the Promo is mandatory.
     */
    private boolean mIsPromoMandatory;

    /**
     * Whether Mandatory Promo acceptance is being animated.
     */
    private boolean mIsAnimatingMandatoryPromoAcceptance;

    /**
     * @return Whether the Panel Promo is visible.
     */
    protected boolean isPromoVisible() {
        return mIsPromoVisible;
    }

    /**
     * Notifies that the acceptance animation has finished.
     */
    protected void onPromoAcceptanceAnimationFinished() {
        mIsAnimatingMandatoryPromoAcceptance = false;

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

    /**
     * @return Whether the content can be displayed in the panel.
     */
    public boolean canDisplayContentInPanel() {
        // TODO(pedrosimonetti): add svelte support.
        return !mIsPromoMandatory || mIsAnimatingMandatoryPromoAcceptance;
    }

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
    // order to have the transparent gap at the top of the Promo View. Currently, the Promo View
    // has a gap at the top where a shadow will be rendered. The gap is needed because the shadow
    // needs to be rendered with the Compositor, which cannot be rendered on top of the Promo's
    // Android View. The suggestion here is to try to use a 9.patch to render the background of
    // the Promo View, which would include the gap at the top. This would allow us simplify the
    // Promo View, by removing the extra nested layout used to leave a gap at the top of the View.

    /**
     * The {@link ContextualSearchOptOutPromo} instance.
     */
    private ContextualSearchOptOutPromo mPromoView;

    /**
     * Whether the Search Promo View is visible.
     */
    private boolean mIsSearchPromoViewVisible = false;

    /**
     * Whether the Promo's acceptance animation is running.
     */
    private boolean mIsAnimatingPromoAcceptance;

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

            if (mResourceLoader != null) {
                mResourceLoader.registerResource(R.id.contextual_search_opt_out_promo,
                        mPromoView.getResourceAdapter());
            }

            mPromoView.setPromoHost(this);

            updatePromoLayout();

            assert mPromoView != null;
        }
    }

    /**
     * Updates the Promo layout.
     */
    protected void updatePromoLayout() {
        final int maximumWidth = getMaximumWidthPx();

        // Adjust size for small Panel.
        if (!isFullWidthSizePanel()) {
            mPromoView.getLayoutParams().width = maximumWidth;
            mPromoView.requestLayout();
        }

        setPromoContentHeightPx(mPromoView.getHeightForGivenWidth(maximumWidth));
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
    private void showPromoViewAtYPosition(float y) {
        if (mPromoView == null
                || mIsSearchPromoViewVisible
                || mIsAnimatingMandatoryPromoAcceptance
                || !isPromoVisible()) return;

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
    protected void hidePromoView() {
        if (mPromoView == null
                || !mIsSearchPromoViewVisible
                || !isPromoVisible()) {
            return;
        }

        mPromoView.setVisibility(View.INVISIBLE);

        mIsSearchPromoViewVisible = false;
    }

    /**
     * Animates the acceptance of the Promo.
     */
    protected void animatePromoAcceptance() {
        hidePromoView();

        if (mIsPromoMandatory) {
            mIsAnimatingMandatoryPromoAcceptance = true;
            getOverlayPanelContent().showContent();
            expandPanel(StateChangeReason.PROMO_ACCEPTED);
        }

        mIsAnimatingPromoAcceptance = true;
        animateProperty(Property.PROMO_VISIBILITY, 1.f, 0.f, BASE_ANIMATION_DURATION_MS);
    }

    /**
     * Updates the UI state for Opt In Promo animation.
     *
     * @param percentage The visibility percentage of the Promo.
     */
    @Override
    protected void setPromoVisibilityForOptInAnimation(float percentage) {
        updatePromoVisibility(percentage);
        updateBarShadow();
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
    // Changes made to the OverlayPanel methods in order to support the Promo.
    // TODO(pedrosimonetti): Re-organize this code once Promo is implemented in its own Control.
    // ============================================================================================

    @Override
    protected void setPanelHeight(float height) {
        // As soon as we resize the Panel to a different height than the expanded one,
        // then we should hide the Promo View once the snapshot will be shown in its place.
        if (height != getPanelHeightFromState(PanelState.EXPANDED)) {
            hidePromoView();
        }

        super.setPanelHeight(height);
    }

    @Override
    protected PanelState getProjectedState(float velocity) {
        PanelState projectedState = super.getProjectedState(velocity);

        // Prevent the fling gesture from moving the Panel from PEEKED to MAXIMIZED if the Panel
        // Promo is available and we are running in full screen panel mode. This is to make sure
        // the Promo will be visible, considering that the EXPANDED state is the only one that will
        // show the Promo in full screen panel mode. In narrow panel UI the Promo is visible in
        // maximized so this project state change is not needed.
        if (isPromoVisible()
                && projectedState == PanelState.MAXIMIZED
                && getPanelState() == PanelState.PEEKED) {
            projectedState = PanelState.EXPANDED;
        }

        return projectedState;
    }

    @Override
    public float getContentY() {
        return getOffsetY() + getBarContainerHeight() + getPromoHeightPx() * mPxToDp;
    }

    @Override
    protected float getBarContainerHeight() {
        return getBarHeight() + getPeekPromoHeight();
    }

    @Override
    protected float getPeekedHeight() {
        return getBarHeightPeeking() + getPeekPromoHeightPeekingPx() * mPxToDp;
    }

    @Override
    protected float calculateBarShadowOpacity() {
        float barShadowOpacity = 0.f;
        float barShadowHeightPx = 9.f / mPxToDp;
        if (getPromoHeightPx() > 0.f) {
            float threshold = 2 * barShadowHeightPx;
            barShadowOpacity = getPromoHeightPx() > barShadowHeightPx ? 1.f
                    : MathUtils.interpolate(0.f, 1.f, getPromoHeightPx() / threshold);
        }
        return barShadowOpacity;
    }
}
