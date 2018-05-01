// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.content.Context;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.View;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.widget.ListMenuButton;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.Collections;
import java.util.List;

/**
 * A mediator for the contextual suggestions UI component responsible for interacting with
 * the contextual suggestions C++ components (via a bridge), updating the model, and communicating
 * with the component coordinator(s).
 */
class ContextualSuggestionsMediator
        implements EnabledStateMonitor.Observer, FetchHelper.Delegate, ListMenuButton.Delegate {
    private final Profile mProfile;
    private final TabModelSelector mTabModelSelector;
    private final ContextualSuggestionsCoordinator mCoordinator;
    private final ContextualSuggestionsModel mModel;
    private final ChromeFullscreenManager mFullscreenManager;
    private final View mIphParentView;
    private final EnabledStateMonitor mEnabledStateMonitor;

    private @Nullable ContextualSuggestionsSource mSuggestionsSource;
    private @Nullable FetchHelper mFetchHelper;
    private @Nullable String mCurrentRequestUrl;
    private @Nullable BottomSheetObserver mSheetObserver;
    private @Nullable TextBubble mHelpBubble;

    private boolean mDidSuggestionsShowForTab;
    private boolean mHasRecordedPeekEventForTab;

    /** Whether the content sheet is observed to be opened for the first time. */
    private boolean mHasSheetBeenOpened;

    /**
     * Construct a new {@link ContextualSuggestionsMediator}.
     * @param profile The regular {@link Profile}.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param fullscreenManager The {@link ChromeFullscreenManager} to listen for browser controls
     *                          events.
     * @param coordinator The {@link ContextualSuggestionsCoordinator} for the component.
     * @param model The {@link ContextualSuggestionsModel} for the component.
     * @param iphParentView The parent {@link View} used to anchor an in-product help bubble.
     */
    ContextualSuggestionsMediator(Profile profile, TabModelSelector tabModelSelector,
            ChromeFullscreenManager fullscreenManager, ContextualSuggestionsCoordinator coordinator,
            ContextualSuggestionsModel model, View iphParentView) {
        mProfile = profile;
        mTabModelSelector = tabModelSelector;
        mCoordinator = coordinator;
        mModel = model;
        mFullscreenManager = fullscreenManager;
        mIphParentView = iphParentView;

        // Create a state monitor that will alert this mediator if the enabled state for contextual
        // suggestions changes.
        mEnabledStateMonitor =
                ContextualSuggestionsDependencyFactory.getInstance().createEnabledStateMonitor(
                        this);

        fullscreenManager.addListener(new FullscreenListener() {
            @Override
            public void onContentOffsetChanged(float offset) {}

            @Override
            public void onControlsOffsetChanged(
                    float topOffset, float bottomOffset, boolean needsAnimate) {
                // When the controls scroll completely off-screen, the suggestions are "shown" but
                // remain hidden since their offset from the bottom of the screen is determined by
                // the top controls.
                if (!mDidSuggestionsShowForTab && mModel.hasSuggestions()
                        && areBrowserControlsHidden() && mSuggestionsSource != null) {
                    showContentInSheet();
                }
            }

            @Override
            public void onToggleOverlayVideoMode(boolean enabled) {}

            @Override
            public void onBottomControlsHeightChanged(int bottomControlsHeight) {}
        });
    }

    /** Destroys the mediator. */
    void destroy() {
        mEnabledStateMonitor.destroy();

        if (mFetchHelper != null) {
            mFetchHelper.destroy();
            mFetchHelper = null;
        }

        if (mSuggestionsSource != null) {
            mSuggestionsSource.destroy();
            mSuggestionsSource = null;
        }

        if (mHelpBubble != null) mHelpBubble.dismiss();
    }

    /** Called when accessibility mode changes. */
    void onAccessibilityModeChanged() {
        mEnabledStateMonitor.onAccessibilityModeChanged();
    }

    /**
     * @return Whether the browser controls are currently completely hidden.
     */
    private boolean areBrowserControlsHidden() {
        return MathUtils.areFloatsEqual(-mFullscreenManager.getTopControlOffset(),
                mFullscreenManager.getTopControlsHeight());
    }

    @Override
    public void onEnabledStateChanged(boolean enabled) {
        if (enabled) {
            mSuggestionsSource = ContextualSuggestionsDependencyFactory.getInstance()
                                         .createContextualSuggestionsSource(mProfile);
            mFetchHelper = ContextualSuggestionsDependencyFactory.getInstance().createFetchHelper(
                    this, mTabModelSelector);
        } else {
            clearSuggestions();

            if (mFetchHelper != null) {
                mFetchHelper.destroy();
                mFetchHelper = null;
            }

            if (mSuggestionsSource != null) {
                mSuggestionsSource.destroy();
                mSuggestionsSource = null;
            }
        }
    }

    @Override
    public void onSettingsStateChanged(boolean enabled) {}

    @Override
    public void requestSuggestions(String url) {
        // Guard against null tabs when requesting suggestions. https://crbug.com/836672.
        if (mTabModelSelector.getCurrentTab() == null
                || mTabModelSelector.getCurrentTab().getWebContents() == null) {
            assert false;
            return;
        }

        reportEvent(ContextualSuggestionsEvent.FETCH_REQUESTED);
        mCurrentRequestUrl = url;
        mSuggestionsSource.fetchSuggestions(url, (suggestionsResult) -> {
            if (mSuggestionsSource == null) return;

            // Avoiding double fetches causing suggestions for incorrect context.
            if (!TextUtils.equals(url, mCurrentRequestUrl)) return;

            List<ContextualSuggestionsCluster> clusters = suggestionsResult.getClusters();

            if (clusters.size() > 0 && clusters.get(0).getSuggestions().size() > 0) {
                preloadContentInSheet(
                        generateClusterList(clusters), suggestionsResult.getPeekText());
                // If the controls are already off-screen, show the suggestions immediately so they
                // are available on reverse scroll.
                if (areBrowserControlsHidden()) showContentInSheet();
            }
        });
    }

    @Override
    public void clearState() {
        clearSuggestions();
    }

    @Override
    public void reportFetchDelayed(WebContents webContents) {
        if (mTabModelSelector.getCurrentTab() != null
                && mTabModelSelector.getCurrentTab().getWebContents() == webContents) {
            reportEvent(ContextualSuggestionsEvent.FETCH_DELAYED);
        }
    }

    // ListMenuButton.Delegate implementation.
    @Override
    public ListMenuButton.Item[] getItems() {
        final Context context = ContextUtils.getApplicationContext();
        return new ListMenuButton.Item[] {
                new ListMenuButton.Item(context, R.string.menu_preferences, true),
                new ListMenuButton.Item(context, R.string.menu_send_feedback, true)};
    }

    @Override
    public void onItemSelected(ListMenuButton.Item item) {
        if (item.getTextId() == R.string.menu_preferences) {
            mCoordinator.showSettings();
        } else if (item.getTextId() == R.string.menu_send_feedback) {
            mCoordinator.showFeedback();
        } else {
            assert false : "Unhandled item detected.";
        }
    }

    /**
     * Called when suggestions are cleared either due to the user explicitly dismissing
     * suggestions via the close button or due to the FetchHelper signaling state should
     * be cleared.
     */
    private void clearSuggestions() {
        // TODO(twellington): Does this signal need to go back to FetchHelper?
        mDidSuggestionsShowForTab = false;
        mHasRecordedPeekEventForTab = false;
        mHasSheetBeenOpened = false;
        mModel.setClusterList(new ClusterList(Collections.emptyList()));
        mModel.setCloseButtonOnClickListener(null);
        mModel.setMenuButtonVisibility(false);
        mModel.setMenuButtonAlpha(0f);
        mModel.setMenuButtonDelegate(null);
        mModel.setDefaultToolbarClickListener(null);
        mModel.setTitle(null);
        mCoordinator.removeSuggestions();
        mCurrentRequestUrl = "";

        if (mSuggestionsSource != null) mSuggestionsSource.clearState();

        if (mSheetObserver != null) {
            mCoordinator.removeBottomSheetObserver(mSheetObserver);
        }

        if (mHelpBubble != null) mHelpBubble.dismiss();
    }

    private void preloadContentInSheet(ClusterList clusters, String title) {
        if (mSuggestionsSource == null) return;

        mModel.setClusterList(clusters);
        mModel.setCloseButtonOnClickListener(view -> {
            TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(
                    EventConstants.CONTEXTUAL_SUGGESTIONS_DISMISSED);
            reportEvent(ContextualSuggestionsEvent.UI_CLOSED);

            clearSuggestions();
        });
        mModel.setMenuButtonVisibility(false);
        mModel.setMenuButtonAlpha(0f);
        mModel.setMenuButtonDelegate(this);
        mModel.setDefaultToolbarClickListener(view -> mCoordinator.expandBottomSheet());
        mModel.setTitle(title);
        mCoordinator.preloadContentInSheet();
    }

    private void showContentInSheet() {
        mDidSuggestionsShowForTab = true;

        mSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetFullyPeeked() {
                if (mHasRecordedPeekEventForTab) return;
                assert !mHasSheetBeenOpened;

                mHasRecordedPeekEventForTab = true;
                TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(
                        EventConstants.CONTEXTUAL_SUGGESTIONS_PEEKED);
                reportEvent(ContextualSuggestionsEvent.UI_PEEK_REVERSE_SCROLL);

                maybeShowHelpBubble();
            }

            @Override
            public void onSheetOffsetChanged(float heightFraction) {
                if (mHelpBubble != null) mHelpBubble.dismiss();
            }

            @Override
            public void onSheetOpened(@StateChangeReason int reason) {
                if (!mHasSheetBeenOpened) {
                    mHasSheetBeenOpened = true;
                    TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(
                            EventConstants.CONTEXTUAL_SUGGESTIONS_OPENED);
                    mCoordinator.showSuggestions(mSuggestionsSource);
                    reportEvent(ContextualSuggestionsEvent.UI_OPENED);
                }
                mModel.setMenuButtonVisibility(true);
            }

            @Override
            public void onSheetClosed(int reason) {
                mModel.setMenuButtonVisibility(false);
            }

            @Override
            public void onTransitionPeekToHalf(float transitionFraction) {
                mModel.setMenuButtonAlpha(transitionFraction);
            }
        };

        mCoordinator.addBottomSheetObserver(mSheetObserver);
        mCoordinator.showContentInSheet();
    }

    private void maybeShowHelpBubble() {
        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
        if (!tracker.shouldTriggerHelpUI(FeatureConstants.CONTEXTUAL_SUGGESTIONS_FEATURE)) {
            return;
        }

        ViewRectProvider rectProvider = new ViewRectProvider(mIphParentView);
        rectProvider.setInsetPx(0,
                mIphParentView.getResources().getDimensionPixelSize(R.dimen.toolbar_shadow_height),
                0, 0);
        mHelpBubble = new TextBubble(mIphParentView.getContext(), mIphParentView,
                R.string.contextual_suggestions_in_product_help,
                R.string.contextual_suggestions_in_product_help, rectProvider);

        mHelpBubble.setDismissOnTouchInteraction(true);
        mHelpBubble.addOnDismissListener(() -> {
            tracker.dismissed(FeatureConstants.CONTEXTUAL_SUGGESTIONS_FEATURE);
            mHelpBubble = null;
        });

        mHelpBubble.show();
    }

    private void reportEvent(@ContextualSuggestionsEvent int event) {
        if (mTabModelSelector.getCurrentTab() == null
                || mTabModelSelector.getCurrentTab().getWebContents() == null) {
            // This method is not expected to be called if the current tab or webcontents are null.
            // If this assert is hit, please alert someone on the Chrome Explore on Content team.
            // See https://crbug.com/836672.
            assert false;
            return;
        }

        mSuggestionsSource.reportEvent(mTabModelSelector.getCurrentTab().getWebContents(), event);
    }

    private ClusterList generateClusterList(List<ContextualSuggestionsCluster> clusters) {
        for (ContextualSuggestionsCluster cluster : clusters) {
            cluster.buildChildren();
        }

        return new ClusterList(clusters);
    }

    @VisibleForTesting
    void showContentInSheetForTesting() {
        showContentInSheet();
    }

    @VisibleForTesting
    TextBubble getHelpBubbleForTesting() {
        return mHelpBubble;
    }
}
