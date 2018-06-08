// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.api.scope.FeedProcessScope;
import com.google.android.libraries.feed.host.config.Configuration;
import com.google.android.libraries.feed.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.host.config.DebugBehavior;
import com.google.android.libraries.feed.hostimpl.logging.LoggingApiImpl;

import org.chromium.chrome.browser.profiles.Profile;

import java.util.concurrent.Executors;

/**
 * Holds singleton {@link FeedProcessScope}.
 */
public class FeedProcessScopeFactory {
    private static FeedProcessScope sFeedProcessScope;

    /**
     * @return The shared {@link FeedProcessScope} instance.
     */
    public static FeedProcessScope getFeedProcessScope() {
        if (sFeedProcessScope == null) {
            Profile profile = Profile.getLastUsedProfile().getOriginalProfile();
            Configuration configHostApi =
                    new Configuration.Builder()
                            .put(ConfigKey.FEED_SERVER_HOST, "https://www.google.com")
                            .put(ConfigKey.FEED_SERVER_PATH_AND_PARAMS,
                                    "/httpservice/noretry/NowStreamService/FeedQuery")
                            .put(ConfigKey.SESSION_LIFETIME_MS, 300000L)
                            .build();
            sFeedProcessScope =
                    new FeedProcessScope
                            .Builder(configHostApi, Executors.newSingleThreadExecutor(),
                                    new LoggingApiImpl(), new FeedNetworkBridge(profile),
                                    new FeedSchedulerBridge(profile), DebugBehavior.SILENT)
                            .build();
        }
        return sFeedProcessScope;
    }
}
