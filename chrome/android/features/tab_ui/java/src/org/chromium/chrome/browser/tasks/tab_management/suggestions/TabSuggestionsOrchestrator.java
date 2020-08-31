// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Represents the entry point for the TabSuggestions component. Responsible for
 * registering and invoking the different {@link TabSuggestionsFetcher}.
 */
public class TabSuggestionsOrchestrator implements TabSuggestions, Destroyable {
    public static final String TAB_SUGGESTIONS_UMA_PREFIX = "TabSuggestionsOrchestrator";
    private static final String LAST_TIMESTAMP_KEY = "LastTimestamp";
    private static final String BACKOFF_COUNT_KEY = "BackoffCountKey";
    private static final String BACKOFF_IDX_KEY = "BackoffIdxKey";
    private static final long[] BACKOFF_AMOUNTS = {TimeUnit.MINUTES.toMillis(1),
            TimeUnit.MINUTES.toMillis(30), TimeUnit.HOURS.toMillis(1), TimeUnit.HOURS.toMillis(2),
            TimeUnit.HOURS.toMillis(12), TimeUnit.DAYS.toMillis(1), TimeUnit.DAYS.toMillis(2),
            TimeUnit.DAYS.toMillis(7), TimeUnit.DAYS.toMillis(10)};
    private static final String TAG = "TabSuggestDetailed";
    private static final int MIN_CLOSE_SUGGESTIONS_THRESHOLD = 3;
    private static final String SHARED_PREFERENCES_ID = "TabsuggestionsPreferences";

    protected TabContextObserver mTabContextObserver;
    protected TabSuggestionFeedback mTabSuggestionFeedback;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final SharedPreferences mSharedPreferences;
    private List<TabSuggestionsFetcher> mTabSuggestionsFetchers;
    private List<TabSuggestion> mPrefetchedResults = new LinkedList<>();
    private TabContext mPrefetchedTabContext;
    private TabModelSelector mTabModelSelector;
    private ObserverList<TabSuggestionsObserver> mTabSuggestionsObservers;
    private int mRemainingFetchers;

    public TabSuggestionsOrchestrator(
            TabModelSelector selector, ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        this(selector, activityLifecycleDispatcher,
                ContextUtils.getApplicationContext().getSharedPreferences(
                        SHARED_PREFERENCES_ID, Context.MODE_PRIVATE));
    }

    @VisibleForTesting
    TabSuggestionsOrchestrator(TabModelSelector selector,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            SharedPreferences sharedPreferences) {
        mTabModelSelector = selector;
        mTabSuggestionsFetchers = new LinkedList<>();
        mTabSuggestionsFetchers.add(new TabSuggestionsClientFetcher());
        mTabSuggestionsObservers = new ObserverList<>();
        mTabContextObserver = new TabContextObserver(selector) {
            @Override
            public void onTabContextChanged(@TabContextChangeReason int changeReason) {
                synchronized (mPrefetchedResults) {
                    if (mPrefetchedTabContext != null) {
                        for (TabSuggestionsObserver tabSuggestionsObserver :
                                mTabSuggestionsObservers) {
                            tabSuggestionsObserver.onTabSuggestionInvalidated();
                        }
                    }
                }
                prefetchSuggestions();
            }
        };
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        activityLifecycleDispatcher.register(this);
        mSharedPreferences = sharedPreferences;
    }

    protected void setUseBaselineTabSuggestionsForTesting() {
        for (TabSuggestionsFetcher fetcher : mTabSuggestionsFetchers) {
            if (fetcher instanceof TabSuggestionsClientFetcher) {
                ((TabSuggestionsClientFetcher) fetcher).setUseBaselineTabSuggestionsForTesting();
            }
        }
    }

    private List<TabSuggestion> aggregateResults(List<TabSuggestion> tabSuggestions) {
        List<TabSuggestion> aggregated = new LinkedList<>();
        for (TabSuggestion tabSuggestion : tabSuggestions) {
            switch (tabSuggestion.getAction()) {
                case TabSuggestion.TabSuggestionAction.CLOSE:
                    if (tabSuggestion.getTabsInfo().size() >= MIN_CLOSE_SUGGESTIONS_THRESHOLD) {
                        aggregated.add(tabSuggestion);
                    }
                    break;
                case TabSuggestion.TabSuggestionAction.GROUP:
                    if (!tabSuggestion.getTabsInfo().isEmpty()) {
                        aggregated.add(tabSuggestion);
                    }
                    break;
                default:
                    android.util.Log.e(
                            TAG, String.format("Unknown action: %d", tabSuggestion.getAction()));
                    break;
            }
        }
        Collections.shuffle(aggregated);
        return aggregated;
    }

    @Override
    public void destroy() {
        mTabContextObserver.destroy();
        mActivityLifecycleDispatcher.unregister(this);
    }

    /**
     * Acquire suggestions and store so suggestions are available for the UI
     * thread on demand.
     */
    protected void prefetchSuggestions() {
        if (isBackoffEnabled()) {
            return;
        }
        TabContext tabContext = TabContext.createCurrentContext(mTabModelSelector);
        synchronized (mPrefetchedResults) {
            mRemainingFetchers = 0;
            mPrefetchedTabContext = tabContext;
            mPrefetchedResults = new LinkedList<>();
            for (TabSuggestionsFetcher tabSuggestionsFetcher : mTabSuggestionsFetchers) {
                if (tabSuggestionsFetcher.isEnabled()) {
                    mRemainingFetchers++;
                    tabSuggestionsFetcher.fetch(tabContext, res -> prefetchCallback(res));
                }
            }
        }
    }

    private boolean isBackoffEnabled() {
        // TODO(crbug.com/1051709) expand to record/read and respond
        // to close/group separately and determine if we can use
        // shared prefs folder.
        synchronized (mSharedPreferences) {
            long lastTimestamp = mSharedPreferences.getLong(LAST_TIMESTAMP_KEY, -1);
            // No lastTimestamp means no dismissals have been recorded, so we
            // don't backoff
            if (lastTimestamp == -1) {
                return false;
            }
            long backoffCount = mSharedPreferences.getLong(BACKOFF_COUNT_KEY, -1);
            // If counting down from the backoff amount is finished i.e. the count
            // is below 0, enough time has elapsed and suggestions can be provided
            // again
            if (backoffCount <= 0) {
                return false;
            }
            long currentTime = System.currentTimeMillis();
            // Decrement time elapsed since last update to the backoff count
            backoffCount -= currentTime - lastTimestamp;
            Editor editor = mSharedPreferences.edit();
            editor.putLong(LAST_TIMESTAMP_KEY, currentTime);
            editor.putLong(BACKOFF_COUNT_KEY, backoffCount);
            editor.apply();
            // If the backoff count is above zero, continue to wait
            // and don't provide suggestions
            return backoffCount > 0;
        }
    }

    private void recordDismissalBackoff() {
        synchronized (mSharedPreferences) {
            int backoffIdx = Math.min(
                    mSharedPreferences.getInt(BACKOFF_IDX_KEY, 0), BACKOFF_AMOUNTS.length - 1);
            Editor editor = mSharedPreferences.edit();
            editor.putLong(BACKOFF_COUNT_KEY, BACKOFF_AMOUNTS[backoffIdx]);
            editor.putInt(BACKOFF_IDX_KEY, backoffIdx + 1);
            editor.putLong(LAST_TIMESTAMP_KEY, System.currentTimeMillis());
            editor.apply();
        }
    }

    private void prefetchCallback(TabSuggestionsFetcherResults suggestions) {
        synchronized (mPrefetchedResults) {
            // If the tab context has changed since the fetchers were used,
            // we simply ignore the result as it is no longer relevant.
            if (suggestions.tabContext.equals(mPrefetchedTabContext)) {
                mRemainingFetchers--;
                mPrefetchedResults.addAll(suggestions.tabSuggestions);
                if (mRemainingFetchers == 0) {
                    for (TabSuggestionsObserver tabSuggestionsObserver : mTabSuggestionsObservers) {
                        tabSuggestionsObserver.onNewSuggestion(aggregateResults(mPrefetchedResults),
                                res -> onTabSuggestionFeedback(res));
                    }
                }
            }
        }
    }

    @Override
    public void addObserver(TabSuggestionsObserver tabSuggestionsObserver) {
        mTabSuggestionsObservers.addObserver(tabSuggestionsObserver);
    }

    @Override
    public void removeObserver(TabSuggestionsObserver tabSuggestionsObserver) {
        mTabSuggestionsObservers.removeObserver(tabSuggestionsObserver);
    }

    public void onTabSuggestionFeedback(TabSuggestionFeedback tabSuggestionFeedback) {
        if (tabSuggestionFeedback == null) {
            Log.e(TAG, "TabSuggestionFeedback is null");
            return;
        }
        // Record TabSuggestionFeedback for testing purposes
        mTabSuggestionFeedback = tabSuggestionFeedback;

        if (tabSuggestionFeedback.tabSuggestionResponse
                == TabSuggestionFeedback.TabSuggestionResponse.NOT_CONSIDERED) {
            RecordUserAction.record("TabsSuggestions.Close.SuggestionsReview.Dismissed");
            recordDismissalBackoff();
            return;
        } else {
            RecordUserAction.record("TabsSuggestions.Close.SuggestionsReview.Accepted");
            if (tabSuggestionFeedback.tabSuggestionResponse
                    == TabSuggestionFeedback.TabSuggestionResponse.ACCEPTED) {
                RecordUserAction.record("TabsSuggestions.Close.Accepted");
            } else {
                RecordUserAction.record("TabsSuggestions.Close.Dismissed");
                return;
            }
        }

        Set<Integer> suggestedTabIds = new HashSet<>();
        for (TabContext.TabInfo tabInfo : tabSuggestionFeedback.tabSuggestion.getTabsInfo()) {
            suggestedTabIds.add(tabInfo.id);
        }

        int numSelectFromSuggestion = 0;
        int numSelectOutsideSuggestion = 0;
        for (int selectedTabId : tabSuggestionFeedback.selectedTabIds) {
            if (suggestedTabIds.contains(selectedTabId)) {
                numSelectFromSuggestion++;
            } else {
                numSelectOutsideSuggestion++;
            }
        }
        int numChanged = tabSuggestionFeedback.tabSuggestion.getTabsInfo().size()
                - numSelectFromSuggestion + numSelectOutsideSuggestion;
        // This was previously TabsSuggestions.Close.NumSuggestionsChanged
        RecordHistogram.recordCount100Histogram(
                "Tabs.Suggestions.Close.NumSuggestionsChanged", numChanged);
    }
}
