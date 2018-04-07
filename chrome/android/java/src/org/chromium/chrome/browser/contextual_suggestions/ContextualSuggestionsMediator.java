// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.content.Context;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.View;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.widget.Toast;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A mediator for the contextual suggestions UI component responsible for interacting with
 * the contextual suggestions C++ components (via a bridge), updating the model, and communicating
 * with the component coordinator(s).
 */
class ContextualSuggestionsMediator implements EnabledStateMonitor.Observer, FetchHelper.Delegate {
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

    /**
     * Construct a new {@link ContextualSuggestionsMediator}.
     * @param context The {@link Context} used to retrieve resources.
     * @param profile The regular {@link Profile}.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param fullscreenManager The {@link ChromeFullscreenManager} to listen for browser controls
     *                          events.
     * @param coordinator The {@link ContextualSuggestionsCoordinator} for the component.
     * @param model The {@link ContextualSuggestionsModel} for the component.
     * @param iphParentView The parent {@link View} used to anchor an in-product help bubble.
     */
    ContextualSuggestionsMediator(Context context, Profile profile,
            TabModelSelector tabModelSelector, ChromeFullscreenManager fullscreenManager,
            ContextualSuggestionsCoordinator coordinator, ContextualSuggestionsModel model,
            View iphParentView) {
        mProfile = profile;
        mTabModelSelector = tabModelSelector;
        mCoordinator = coordinator;
        mModel = model;
        mFullscreenManager = fullscreenManager;
        mIphParentView = iphParentView;

        // Create a state monitor that will alert this mediator if the enabled state for contextual
        // suggestions changes.
        mEnabledStateMonitor = new EnabledStateMonitor(this);

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
            mSuggestionsSource = new ContextualSuggestionsSource(mProfile);
            mFetchHelper = new FetchHelper(this, mTabModelSelector);
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
    public void requestSuggestions(String url) {
        mCurrentRequestUrl = url;
        mSuggestionsSource.getBridge().fetchSuggestions(url, (suggestionsResult) -> {
            if (mSuggestionsSource == null) return;

            // Avoiding double fetches causing suggestions for incorrect context.
            if (!TextUtils.equals(url, mCurrentRequestUrl)) return;

            List<ContextualSuggestionsCluster> clusters = suggestionsResult.getClusters();

            if (clusters.size() > 0 && clusters.get(0).getSuggestions().size() > 0) {
                Toast.makeText(ContextUtils.getApplicationContext(),
                             clusters.size() + " clusters fetched", Toast.LENGTH_SHORT)
                        .show();

                preloadContentInSheet(
                        generateClusterList(clusters), suggestionsResult.getPeekText());
                // If the controls are already off-screen, show the suggestions immediately so they
                // are available on reverse scroll.
                if (areBrowserControlsHidden()) showContentInSheet();
            } else {
                Toast.makeText(ContextUtils.getApplicationContext(), "No suggestions",
                             Toast.LENGTH_SHORT)
                        .show();
            }
        });
    }

    @Override
    public void clearState() {
        clearSuggestions();
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
        mModel.setClusterList(new ClusterList(Collections.emptyList()));
        mModel.setCloseButtonOnClickListener(null);
        mModel.setTitle(null);
        mCoordinator.removeSuggestions();
        mCurrentRequestUrl = "";

        if (mSheetObserver != null) {
            mCoordinator.removeBottomSheetObserver(mSheetObserver);
        }

        if (mHelpBubble != null) mHelpBubble.dismiss();
    }

    private void preloadContentInSheet(ClusterList clusters, String title) {
        if (mSuggestionsSource == null) return;

        mModel.setClusterList(clusters);
        mModel.setCloseButtonOnClickListener(view -> {
            clearSuggestions();
            TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(
                    EventConstants.CONTEXTUAL_SUGGESTIONS_DISMISSED);
        });
        mModel.setTitle(title);
        mCoordinator.preloadContentInSheet();
    }

    private void showContentInSheet() {
        mDidSuggestionsShowForTab = true;

        mSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetFullyPeeked() {
                if (mHasRecordedPeekEventForTab) return;

                mHasRecordedPeekEventForTab = true;
                TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(
                        EventConstants.CONTEXTUAL_SUGGESTIONS_PEEKED);
                maybeShowHelpBubble();
            }

            @Override
            public void onSheetOffsetChanged(float heightFraction) {
                if (mHelpBubble != null) mHelpBubble.dismiss();
            }

            @Override
            public void onSheetOpened(@StateChangeReason int reason) {
                TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(
                        EventConstants.CONTEXTUAL_SUGGESTIONS_OPENED);

                mCoordinator.showSuggestions(mSuggestionsSource);
                mCoordinator.removeBottomSheetObserver(this);
                mSheetObserver = null;
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

    // TODO(twellington): Remove after clusters are returned from the backend.
    private ClusterList generateClusterList(List<ContextualSuggestionsCluster> clusters) {
        if (clusters.size() != 1) {
            for (ContextualSuggestionsCluster cluster : clusters) {
                cluster.buildChildren();
            }

            return new ClusterList(clusters);
        }

        List<SnippetArticle> suggestions = clusters.get(0).getSuggestions();
        List<ContextualSuggestionsCluster> newClusters = new ArrayList<>();
        int clusterSize = suggestions.size() >= 6 ? 3 : 2;
        int numClusters = suggestions.size() < 4 ? 1 : suggestions.size() / clusterSize;
        int currentSuggestion = 0;

        // Construct a list of clusters.
        for (int i = 0; i < numClusters; i++) {
            ContextualSuggestionsCluster cluster = new ContextualSuggestionsCluster(
                    i != 0 ? suggestions.get(currentSuggestion).mTitle : "");

            for (int j = 0; j < clusterSize; j++) {
                cluster.getSuggestions().add(suggestions.get(currentSuggestion));
                currentSuggestion++;
            }

            newClusters.add(cluster);
        }

        // Add the remaining suggestions to the last cluster.
        while (currentSuggestion < suggestions.size()) {
            newClusters.get(newClusters.size() - 1)
                    .getSuggestions()
                    .add(suggestions.get(currentSuggestion));
            currentSuggestion++;
        }

        for (ContextualSuggestionsCluster cluster : newClusters) {
            cluster.buildChildren();
        }

        return new ClusterList(newClusters);
    }
}
