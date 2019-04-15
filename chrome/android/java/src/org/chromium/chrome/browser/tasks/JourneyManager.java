// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.NonNull;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.content_public.browser.NavigationHandle;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * Manages Journey related signals, specifically those related to tab engagement.
 */
public class JourneyManager implements Destroyable {
    private static final long INVALID_START_TIME = -1;

    @VisibleForTesting
    static final String PREFS_FILE = "last_engagement_for_tab_id_pref";

    @VisibleForTesting
    static final String TAB_REVISIT_METRIC = "Tabs.TimeSinceLastView.OnTabView";

    @VisibleForTesting
    static final String TAB_CLOSE_METRIC = "Tabs.TimeSinceLastView.OnTabClose";

    // We track this in seconds because UMA can only handle 32-bit signed integers, which 45 days
    // will overflow.
    private static final int MAX_ENGAGEMENT_TIME_S = (int) TimeUnit.DAYS.toSeconds(45);

    private final TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private final TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final EngagementTimeUtil mEngagementTimeUtil;

    private Map<Integer, Boolean> mDidFirstPaintPerTab = new HashMap<>();

    public JourneyManager(TabModelSelector selector,
            @NonNull ActivityLifecycleDispatcher dispatcher,
            EngagementTimeUtil engagementTimeUtil) {
        if (!ChromeVersionInfo.isLocalBuild() && !ChromeVersionInfo.isCanaryBuild()
                && !ChromeVersionInfo.isDevBuild()) {
            // We do not want this in beta/stable until it's no longer backed by SharedPreferences.
            mTabModelSelectorTabObserver = null;
            mTabModelSelectorTabModelObserver = null;
            mLifecycleDispatcher = null;
            mEngagementTimeUtil = null;
            return;
        }

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onShown(Tab tab, @TabSelectionType int type) {
                if (type != TabSelectionType.FROM_USER) return;

                maybeRecordEngagementMetric(tab, TAB_REVISIT_METRIC);

                handleTabEngagementStarted(tab);
            }

            @Override
            public void onHidden(Tab tab, @Tab.TabHidingType int reason) {
                handleTabEngagementStopped(tab);
            }

            @Override
            public void onClosingStateChanged(Tab tab, boolean closing) {
                if (!closing) return;

                maybeRecordEngagementMetric(tab, TAB_CLOSE_METRIC);
            }

            @Override
            public void onDidStartNavigation(Tab tab, NavigationHandle navigationHandle) {
                if (!navigationHandle.isInMainFrame() || navigationHandle.isSameDocument()) return;

                mDidFirstPaintPerTab.put(tab.getId(), false);
            }

            @Override
            public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                mDidFirstPaintPerTab.put(tab.getId(), true);
                handleTabEngagementStarted(tab);
            }
        };

        mTabModelSelectorTabModelObserver = new TabModelSelectorTabModelObserver(selector) {
            @Override
            public void tabClosureCommitted(Tab tab) {
                getPrefs().edit().remove(String.valueOf(tab.getId())).apply();
            }
        };

        mLifecycleDispatcher = dispatcher;
        mLifecycleDispatcher.register(this);

        mEngagementTimeUtil = engagementTimeUtil;
    }

    @Override
    public void destroy() {
        mTabModelSelectorTabObserver.destroy();
        mTabModelSelectorTabModelObserver.destroy();
        mLifecycleDispatcher.unregister(this);
    }

    private void handleTabEngagementStarted(Tab tab) {
        long lastEngagementMs = mEngagementTimeUtil.currentTime();

        Boolean didFirstPaint = mDidFirstPaintPerTab.get(tab.getId());
        if (didFirstPaint == null || !didFirstPaint) return;

        storeLastEngagement(tab.getId(), lastEngagementMs);
    }

    private void handleTabEngagementStopped(Tab tab) {
        long lastEngagementMs = mEngagementTimeUtil.currentTime();

        Boolean didFirstPaint = mDidFirstPaintPerTab.get(tab.getId());
        if (didFirstPaint == null || !didFirstPaint) {
            return;
        }

        storeLastEngagement(tab.getId(), lastEngagementMs);
    }

    private void storeLastEngagement(int tabId, long lastEngagementTimestampMs) {
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                getPrefs().edit().putLong(String.valueOf(tabId), lastEngagementTimestampMs).apply();
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {}
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    private SharedPreferences getPrefs() {
        // TODO(mattsimmons): Add a native counterpart to this class and don't write directly to
        //  shared prefs.
        return ContextUtils.getApplicationContext().getSharedPreferences(
                PREFS_FILE, Context.MODE_PRIVATE);
    }

    private long getLastEngagementTimestamp(Tab tab) {
        return getPrefs().getLong(String.valueOf(tab.getId()), INVALID_START_TIME);
    }

    private void maybeRecordEngagementMetric(Tab tab, String name) {
        long lastEngagement = getLastEngagementTimestamp(tab);

        if (lastEngagement == INVALID_START_TIME) return;

        // Compute elapsed time and convert to seconds.
        long elapsedMs = mEngagementTimeUtil.timeSinceLastEngagement(lastEngagement);
        int elapsedSeconds = (int) TimeUnit.MILLISECONDS.toSeconds(elapsedMs);
        RecordHistogram.recordCustomCountHistogram(
                name, elapsedSeconds, 1, MAX_ENGAGEMENT_TIME_S, 50);
    }

    @VisibleForTesting
    public TabObserver getTabModelSelectorTabObserver() {
        return mTabModelSelectorTabObserver;
    }

    @VisibleForTesting
    public TabModelObserver getTabModelSelectorTabModelObserver() {
        return mTabModelSelectorTabModelObserver;
    }
}
