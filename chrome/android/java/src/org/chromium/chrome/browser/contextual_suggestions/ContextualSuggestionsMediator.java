// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.content.Context;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A mediator for the contextual suggestions UI component responsible for interacting with
 * the contextual suggestions C++ components (via a bridge), updating the model, and communicating
 * with the component coordinator(s).
 */
class ContextualSuggestionsMediator implements EnabledStateMonitor.Observer, FetchHelper.Delegate {
    private final Context mContext;
    private final Profile mProfile;
    private final TabModelSelector mTabModelSelector;
    private final ContextualSuggestionsCoordinator mCoordinator;
    private final ContextualSuggestionsModel mModel;
    private final EnabledStateMonitor mEnabledStateMonitor;
    private final ChromeFullscreenManager mFullscreenManager;

    private @Nullable SnippetsBridge mBridge;
    private @Nullable FetchHelper mFetchHelper;
    private @Nullable String mCurrentRequestUrl;
    private @Nullable BottomSheetObserver mSheetObserver;

    private boolean mDidSuggestionsShowForTab;

    /**
     * Construct a new {@link ContextualSuggestionsMediator}.
     * @param context The {@link Context} used to retrieve resources.
     * @param profile The regular {@link Profile}.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param fullscreenManager The {@link ChromeFullscreenManager} to listen for browser controls
     *                          events.
     * @param coordinator The {@link ContextualSuggestionsCoordinator} for the component.
     * @param model The {@link ContextualSuggestionsModel} for the component.
     */
    ContextualSuggestionsMediator(Context context, Profile profile,
            TabModelSelector tabModelSelector, ChromeFullscreenManager fullscreenManager,
            ContextualSuggestionsCoordinator coordinator, ContextualSuggestionsModel model) {
        mContext = context;
        mProfile = profile;
        mTabModelSelector = tabModelSelector;
        mCoordinator = coordinator;
        mModel = model;
        mFullscreenManager = fullscreenManager;

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
                        && areBrowserControlsHidden() && mBridge != null) {
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

        if (mBridge != null) {
            mBridge.destroy();
            mBridge = null;
        }
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
            mBridge = new SnippetsBridge(mProfile);
            mFetchHelper = new FetchHelper(this, mTabModelSelector);
        } else {
            clearSuggestions();

            if (mFetchHelper != null) {
                mFetchHelper.destroy();
                mFetchHelper = null;
            }

            if (mBridge != null) {
                mBridge.destroy();
                mBridge = null;
            }
        }
    }

    @Override
    public void requestSuggestions(String url) {
        mCurrentRequestUrl = url;
        mBridge.fetchContextualSuggestions(url, (suggestions) -> {
            if (mBridge == null) return;

            // Avoiding double fetches causing suggestions for incorrect context.
            if (!TextUtils.equals(url, mCurrentRequestUrl)) return;

            Toast.makeText(ContextUtils.getApplicationContext(),
                         suggestions.size() + " suggestions fetched", Toast.LENGTH_SHORT)
                    .show();

            if (suggestions.size() > 0) {
                preloadContentInSheet(generateClusterList(suggestions), suggestions.get(0).mTitle);
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

    /**
     * Called when suggestions are cleared either due to the user explicitly dismissing
     * suggestions via the close button or due to the FetchHelper signaling state should
     * be cleared.
     */
    private void clearSuggestions() {
        // TODO(twellington): Does this signal need to go back to FetchHelper?
        mDidSuggestionsShowForTab = false;
        mModel.setClusterList(new ClusterList(Collections.emptyList()));
        mModel.setCloseButtonOnClickListener(null);
        mModel.setTitle(null);
        mCoordinator.removeSuggestions();
        mCurrentRequestUrl = "";

        if (mSheetObserver != null) {
            mCoordinator.removeBottomSheetObserver(mSheetObserver);
        }
    }

    private void preloadContentInSheet(ClusterList clusters, String title) {
        if (mBridge == null) return;

        mModel.setClusterList(clusters);
        mModel.setCloseButtonOnClickListener(view -> { clearSuggestions(); });
        mModel.setTitle(mContext.getString(R.string.contextual_suggestions_toolbar_title, title));
        mCoordinator.preloadContentInSheet();
    }

    private void showContentInSheet() {
        mDidSuggestionsShowForTab = true;

        mSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetOpened(@StateChangeReason int reason) {
                mCoordinator.showSuggestions(mBridge);
                mCoordinator.removeBottomSheetObserver(this);
                mSheetObserver = null;
            }
        };

        mCoordinator.addBottomSheetObserver(mSheetObserver);
        mCoordinator.showContentInSheet();
    }

    // TODO(twellington): Remove after clusters are returned from the backend.
    private ClusterList generateClusterList(List<SnippetArticle> suggestions) {
        List<ContextualSuggestionsCluster> clusters = new ArrayList<>();
        int clusterSize = suggestions.size() >= 6 ? 3 : 2;
        int numClusters = suggestions.size() < 4 ? 1 : suggestions.size() / clusterSize;
        int currentSuggestion = 0;

        // Construct a list of clusters.
        for (int i = 0; i < numClusters; i++) {
            ContextualSuggestionsCluster cluster =
                    new ContextualSuggestionsCluster(suggestions.get(currentSuggestion).mTitle);
            if (i == 0) cluster.setShouldShowTitle(false);

            for (int j = 0; j < clusterSize; j++) {
                cluster.getSuggestions().add(suggestions.get(currentSuggestion));
                currentSuggestion++;
            }

            clusters.add(cluster);
        }

        // Add the remaining suggestions to the last cluster.
        while (currentSuggestion < suggestions.size()) {
            clusters.get(clusters.size() - 1)
                    .getSuggestions()
                    .add(suggestions.get(currentSuggestion));
            currentSuggestion++;
        }

        for (ContextualSuggestionsCluster cluster : clusters) {
            cluster.buildChildren();
        }

        return new ClusterList(clusters);
    }
}
