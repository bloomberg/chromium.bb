// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.annotation.TargetApi;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.os.Build;

import org.chromium.base.BuildInfo;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.components.url_formatter.UrlFormatter;

/**
 * Interface for native code to interact with Android notification channels.
 */
public class NotificationSettingsBridge {
    // TODO(awdf): Remove this and check BuildInfo.sdk_int() from native instead, once SdkVersion
    // enum includes Android O.
    @CalledByNative
    static boolean shouldUseChannelSettings() {
        return BuildInfo.isAtLeastO();
    }

    /**
     * Creates a notification channel for the given origin, unless a channel for this origin
     * already exists.
     *
     * @param origin The site origin to be used as the channel name.
     * @param creationTime A string representing the time of channel creation.
     * @param enabled True if the channel should be initially enabled, false if
     *                it should start off as blocked.
     * @return The channel created for this origin.
     */
    @TargetApi(Build.VERSION_CODES.O)
    @CalledByNative
    static SiteChannel createChannel(String origin, long creationTime, boolean enabled) {
        return SiteChannelsManager.getInstance().createSiteChannel(origin, creationTime, enabled);
    }

    @TargetApi(Build.VERSION_CODES.O)
    @CalledByNative
    static @NotificationChannelStatus int getChannelStatus(String channelId) {
        return SiteChannelsManager.getInstance().getChannelStatus(channelId);
    }

    @TargetApi(Build.VERSION_CODES.O)
    @CalledByNative
    static SiteChannel[] getSiteChannels() {
        return SiteChannelsManager.getInstance().getSiteChannels();
    }

    @TargetApi(Build.VERSION_CODES.O)
    @CalledByNative
    static void deleteChannel(String channelId) {
        SiteChannelsManager.getInstance().deleteSiteChannel(channelId);
    }

    /**
     * Helper type for passing site channel objects across the JNI.
     */
    @TargetApi(Build.VERSION_CODES.O)
    public static class SiteChannel {
        private final String mId;
        private final String mOrigin;
        private final long mTimestamp;
        private final @NotificationChannelStatus int mStatus;

        public SiteChannel(String channelId, String origin, long creationTimestamp,
                @NotificationChannelStatus int status) {
            mId = channelId;
            mOrigin = origin;
            mTimestamp = creationTimestamp;
            mStatus = status;
        }

        @CalledByNative("SiteChannel")
        public long getTimestamp() {
            return mTimestamp;
        }

        @CalledByNative("SiteChannel")
        public String getOrigin() {
            return mOrigin;
        }

        @CalledByNative("SiteChannel")
        public @NotificationChannelStatus int getStatus() {
            return mStatus;
        }

        @CalledByNative("SiteChannel")
        public String getId() {
            return mId;
        }

        public NotificationChannel toChannel() {
            NotificationChannel channel = new NotificationChannel(mId,
                    UrlFormatter.formatUrlForSecurityDisplay(mOrigin, false /* showScheme */),
                    mStatus == NotificationChannelStatus.BLOCKED
                            ? NotificationManager.IMPORTANCE_NONE
                            : NotificationManager.IMPORTANCE_DEFAULT);
            channel.setGroup(ChannelDefinitions.CHANNEL_GROUP_ID_SITES);
            return channel;
        }
    }
}
