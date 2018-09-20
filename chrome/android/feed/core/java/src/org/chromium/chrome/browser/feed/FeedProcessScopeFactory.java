// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.support.annotation.Nullable;

import com.google.android.libraries.feed.api.common.ThreadUtils;
import com.google.android.libraries.feed.api.scope.FeedProcessScope;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedAppLifecycleListener;
import com.google.android.libraries.feed.host.config.Configuration;
import com.google.android.libraries.feed.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.host.config.DebugBehavior;
import com.google.android.libraries.feed.host.network.NetworkClient;
import com.google.android.libraries.feed.hostimpl.logging.LoggingApiImpl;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.concurrent.Executors;

/** Holds singleton {@link FeedProcessScope} and some of the scope's host implementations. */
public class FeedProcessScopeFactory {
    private static PrefChangeRegistrar sPrefChangeRegistrar;
    private static FeedProcessScope sFeedProcessScope;
    private static FeedScheduler sFeedScheduler;
    private static FeedOfflineIndicator sFeedOfflineIndicator;

    /** @return The shared {@link FeedProcessScope} instance. Null if the Feed is disabled. */
    public static @Nullable FeedProcessScope getFeedProcessScope() {
        if (sFeedProcessScope == null) {
            initialize();
        }
        return sFeedProcessScope;
    }

    /** @return The {@link FeedScheduler} that was given to the {@link FeedProcessScope}. Null if
     * the Feed is disabled. */
    public static @Nullable FeedScheduler getFeedScheduler() {
        if (sFeedScheduler == null) {
            initialize();
        }
        return sFeedScheduler;
    }

    /** @return The {@link FeedOfflineIndicator} that was given to the {@link FeedProcessScope}.
     * Null if the Feed is disabled. */
    public static @Nullable FeedOfflineIndicator getFeedOfflineIndicator() {
        if (sFeedOfflineIndicator == null) {
            initialize();
        }
        return sFeedOfflineIndicator;
    }

    private static void initialize() {
        assert sFeedProcessScope == null && sFeedScheduler == null && sFeedOfflineIndicator == null;
        if (!PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_SECTION_ENABLED)) {
            return;
        }

        sPrefChangeRegistrar = new PrefChangeRegistrar();
        sPrefChangeRegistrar.addObserver(Pref.NTP_ARTICLES_SECTION_ENABLED,
                FeedProcessScopeFactory::articlesEnabledPrefChange);

        Profile profile = Profile.getLastUsedProfile().getOriginalProfile();
        Configuration configHostApi = createConfiguration();

        FeedSchedulerBridge schedulerBridge = new FeedSchedulerBridge(profile);
        sFeedScheduler = schedulerBridge;
        FeedAppLifecycleListener lifecycleListener =
                new FeedAppLifecycleListener(new ThreadUtils());
        FeedContentStorage contentStorage = new FeedContentStorage(profile);
        FeedJournalStorage journalStorage = new FeedJournalStorage(profile);
        sFeedProcessScope =
                new FeedProcessScope
                        .Builder(configHostApi, Executors.newSingleThreadExecutor(),
                                new LoggingApiImpl(), new FeedNetworkBridge(profile),
                                schedulerBridge, lifecycleListener, DebugBehavior.SILENT,
                                ContextUtils.getApplicationContext())
                        .setContentStorage(contentStorage)
                        .setJournalStorage(journalStorage)
                        .build();
        schedulerBridge.initializeFeedDependencies(
                sFeedProcessScope.getRequestManager(), sFeedProcessScope.getSessionManager());

        sFeedOfflineIndicator =
                new FeedOfflineBridge(profile, sFeedProcessScope.getKnownContentApi());
    }

    private static Configuration createConfiguration() {
        return new Configuration.Builder()
                .put(ConfigKey.FEED_SERVER_HOST, "https://www.google.com")
                .put(ConfigKey.FEED_SERVER_PATH_AND_PARAMS,
                        "/httpservice/noretry/NowStreamService/FeedQuery")
                .put(ConfigKey.SESSION_LIFETIME_MS, 300000L)
                .build();
    }

    /**
     * Creates a {@link FeedProcessScope} using the provided host implementations. Call {@link
     * #clearFeedProcessScopeForTesting()} to reset the FeedProcessScope after testing is complete.
     *
     * @param feedScheduler A {@link FeedScheduler} to use for testing.
     * @param networkClient A {@link NetworkClient} to use for testing.
     * @param feedOfflineIndicator A {@link FeedOfflineIndicator} to use for testing.
     */
    @VisibleForTesting
    static void createFeedProcessScopeForTesting(FeedScheduler feedScheduler,
            NetworkClient networkClient, FeedOfflineIndicator feedOfflineIndicator) {
        Configuration configHostApi = createConfiguration();
        FeedAppLifecycleListener lifecycleListener =
                new FeedAppLifecycleListener(new ThreadUtils());
        sFeedScheduler = feedScheduler;
        sFeedProcessScope = new FeedProcessScope
                                    .Builder(configHostApi, Executors.newSingleThreadExecutor(),
                                            new LoggingApiImpl(), networkClient, sFeedScheduler,
                                            lifecycleListener, DebugBehavior.SILENT,
                                            ContextUtils.getApplicationContext())
                                    .build();
        sFeedOfflineIndicator = feedOfflineIndicator;
    }

    /** Resets the FeedProcessScope after testing is complete. */
    @VisibleForTesting
    static void clearFeedProcessScopeForTesting() {
        destroy();
    }

    private static void articlesEnabledPrefChange() {
        // Should only be subscribed while it was enabled. A change should mean articles are now
        // disabled.
        assert !PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_SECTION_ENABLED);
        destroy();
    }

    /** Clears out all static state. */
    private static void destroy() {
        if (sPrefChangeRegistrar != null) {
            sPrefChangeRegistrar.removeObserver(Pref.NTP_ARTICLES_SECTION_ENABLED);
            sPrefChangeRegistrar.destroy();
            sPrefChangeRegistrar = null;
        }
        if (sFeedProcessScope != null) {
            sFeedProcessScope.onDestroy();
            sFeedProcessScope = null;
        }
        if (sFeedScheduler != null) {
            sFeedScheduler.destroy();
            sFeedScheduler = null;
        }
        if (sFeedOfflineIndicator != null) {
            sFeedOfflineIndicator.destroy();
            sFeedOfflineIndicator = null;
        }
    }
}
