// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

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
import org.chromium.chrome.browser.profiles.Profile;

import java.util.concurrent.Executors;

/**
 * Holds singleton {@link FeedProcessScope} and some of the scope's host implementations.
 */
public class FeedProcessScopeFactory {
    private static FeedProcessScope sFeedProcessScope;
    private static FeedScheduler sFeedScheduler;

    /**
     * @return The shared {@link FeedProcessScope} instance.
     */
    public static FeedProcessScope getFeedProcessScope() {
        if (sFeedProcessScope == null) {
            initialize();
        }
        return sFeedProcessScope;
    }

    /**
     * @return The {@link FeedSchedulerBridge} that was given to the {@link FeedProcessScope}.
     */
    public static FeedScheduler getFeedSchedulerBridge() {
        if (sFeedScheduler == null) {
            initialize();
        }
        return sFeedScheduler;
    }

    private static void initialize() {
        assert sFeedScheduler == null && sFeedProcessScope == null;
        Profile profile = Profile.getLastUsedProfile().getOriginalProfile();
        Configuration configHostApi = createConfiguration();

        FeedSchedulerBridge schedulerBridge = new FeedSchedulerBridge(profile);
        sFeedScheduler = schedulerBridge;
        FeedAppLifecycleListener lifecycleListener =
                new FeedAppLifecycleListener(new ThreadUtils());
        sFeedProcessScope =
                new FeedProcessScope
                        .Builder(configHostApi, Executors.newSingleThreadExecutor(),
                                new LoggingApiImpl(), new FeedNetworkBridge(profile),
                                schedulerBridge, lifecycleListener, DebugBehavior.SILENT,
                                ContextUtils.getApplicationContext())
                        .build();
        schedulerBridge.initializeFeedDependencies(
                sFeedProcessScope.getRequestManager(), sFeedProcessScope.getSessionManager());
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
     * Creates a {@link FeedProcessScope} using the provided {@link FeedScheduler} and
     * {@link NetworkClient}. Call {@link #clearFeedProcessScopeForTesting()} to reset the
     * FeedProcessScope after testing is complete.
     *
     * @param feedScheduler A {@link FeedScheduler} to use for testing.
     * @param networkClient A {@link NetworkClient} to use for testing.
     */
    @VisibleForTesting
    static void createFeedProcessScopeForTesting(
            FeedScheduler feedScheduler, NetworkClient networkClient) {
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
    }

    /** Resets the FeedProcessScope after testing is complete. */
    @VisibleForTesting
    static void clearFeedProcessScopeForTesting() {
        if (sFeedScheduler != null) {
            sFeedScheduler.destroy();
            sFeedScheduler = null;
        }

        sFeedProcessScope = null;
    }
}
