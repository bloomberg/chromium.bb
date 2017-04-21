// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.annotation.TargetApi;
import android.app.NotificationManager;
import android.os.Build;
import android.support.annotation.StringDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * Defines the properties of all notification channels we post notifications to in Android O+.
 *
 * PLEASE NOTE, notification channels appear in system UI and are persisted forever by Android,
 * so should not be added or removed lightly, and the proper deprecation and versioning steps must
 * be taken when doing so. Please read the comments and speak to one of this file's OWNERs when
 * adding/removing a channel.
 */
public class ChannelDefinitions {
    public static final String CHANNEL_ID_BROWSER = "browser";
    public static final String CHANNEL_ID_DOWNLOADS = "downloads";
    public static final String CHANNEL_ID_INCOGNITO = "incognito";
    public static final String CHANNEL_ID_MEDIA = "media";
    public static final String CHANNEL_ID_SITES = "sites";
    static final String CHANNEL_GROUP_ID_GENERAL = "general";
    /**
     * Version number identifying the current set of channels. This must be incremented whenever
     * any channels are added or removed from the set of channels created in
     * {@link ChannelsInitializer#initializeStartupChannels()}.
     */
    static final int CHANNELS_VERSION = 0;

    // To define a new channel, add the channel ID to this StringDef and add a new entry to
    // PredefinedChannels.MAP below with the appropriate channel parameters.
    @StringDef({CHANNEL_ID_BROWSER, CHANNEL_ID_DOWNLOADS, CHANNEL_ID_INCOGNITO, CHANNEL_ID_MEDIA,
            CHANNEL_ID_SITES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ChannelId {}

    @StringDef({CHANNEL_GROUP_ID_GENERAL})
    @Retention(RetentionPolicy.SOURCE)
    @interface ChannelGroupId {}

    // Map defined in static inner class so it's only initialized lazily.
    @TargetApi(Build.VERSION_CODES.N) // for NotificationManager.IMPORTANCE_* constants
    private static class PredefinedChannels {
        /**
         * The set of predefined channels to be initialized on startup. CHANNELS_VERSION must be
         * incremented every time an entry is modified, removed or added to this map.
         */
        static final Map<String, Channel> MAP;
        static {
            Map<String, Channel> map = new HashMap<>();
            map.put(CHANNEL_ID_BROWSER,
                    new Channel(CHANNEL_ID_BROWSER,
                            org.chromium.chrome.R.string.notification_category_browser,
                            NotificationManager.IMPORTANCE_LOW, CHANNEL_GROUP_ID_GENERAL));
            map.put(CHANNEL_ID_DOWNLOADS,
                    new Channel(CHANNEL_ID_DOWNLOADS,
                            org.chromium.chrome.R.string.notification_category_downloads,
                            NotificationManager.IMPORTANCE_LOW, CHANNEL_GROUP_ID_GENERAL));
            map.put(CHANNEL_ID_INCOGNITO,
                    new Channel(CHANNEL_ID_INCOGNITO,
                            org.chromium.chrome.R.string.notification_category_incognito,
                            NotificationManager.IMPORTANCE_LOW, CHANNEL_GROUP_ID_GENERAL));
            map.put(CHANNEL_ID_MEDIA,
                    new Channel(CHANNEL_ID_MEDIA,
                            org.chromium.chrome.R.string.notification_category_media,
                            NotificationManager.IMPORTANCE_LOW, CHANNEL_GROUP_ID_GENERAL));
            map.put(CHANNEL_ID_SITES,
                    new Channel(CHANNEL_ID_SITES,
                            org.chromium.chrome.R.string.notification_category_sites,
                            NotificationManager.IMPORTANCE_DEFAULT, CHANNEL_GROUP_ID_GENERAL));
            MAP = Collections.unmodifiableMap(map);
        }
    }

    // Map defined in static inner class so it's only initialized lazily.
    private static class PredefinedChannelGroups {
        static final Map<String, ChannelGroup> MAP;
        static {
            Map<String, ChannelGroup> map = new HashMap<>();
            map.put(CHANNEL_GROUP_ID_GENERAL,
                    new ChannelGroup(CHANNEL_GROUP_ID_GENERAL,
                            org.chromium.chrome.R.string.notification_category_group_general));
            MAP = Collections.unmodifiableMap(map);
        }
    }

    Set<String> getStartupChannelIds() {
        // CHANNELS_VERSION must be incremented if the set of channels returned here changes.
        return PredefinedChannels.MAP.keySet();
    }

    ChannelGroup getChannelGroupFromId(Channel channel) {
        return PredefinedChannelGroups.MAP.get(channel.mGroupId);
    }

    Channel getChannelFromId(@ChannelId String channelId) {
        return PredefinedChannels.MAP.get(channelId);
    }

    /**
     * Helper class containing notification channel properties.
     */
    public static class Channel {
        @ChannelId
        public final String mId;
        final int mNameResId;
        final int mImportance;
        @ChannelGroupId
        final String mGroupId;

        Channel(@ChannelId String id, int nameResId, int importance,
                @ChannelGroupId String groupId) {
            this.mId = id;
            this.mNameResId = nameResId;
            this.mImportance = importance;
            this.mGroupId = groupId;
        }
    }

    /**
     * Helper class containing notification channel group properties.
     */
    public static class ChannelGroup {
        @ChannelGroupId
        final String mId;
        final int mNameResId;

        ChannelGroup(@ChannelGroupId String id, int nameResId) {
            this.mId = id;
            this.mNameResId = nameResId;
        }
    }
}
