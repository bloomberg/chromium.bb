// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import android.app.NotificationManager;
import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.notifications.NotificationChannelStatus;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationSettingsBridge.SiteChannel;
import org.chromium.chrome.browser.preferences.website.WebsiteAddress;

import java.util.ArrayList;
import java.util.List;

/**
 * Creates/deletes and queries our notification channels for websites.
 */
public class SiteChannelsManager {
    private final NotificationManagerProxy mNotificationManager;

    public static SiteChannelsManager getInstance() {
        return LazyHolder.INSTANCE;
    }

    private static class LazyHolder {
        public static final SiteChannelsManager INSTANCE =
                new SiteChannelsManager(new NotificationManagerProxyImpl(
                        (NotificationManager) ContextUtils.getApplicationContext().getSystemService(
                                Context.NOTIFICATION_SERVICE)));
    }

    @VisibleForTesting
    SiteChannelsManager(NotificationManagerProxy notificationManagerProxy) {
        mNotificationManager = notificationManagerProxy;
    }

    /**
     * Creates a channel for the given origin. The channel will appear within the Sites channel
     * group, with default importance, or no importance if created as blocked.
     * @param origin The site origin, to be used as the channel's user-visible name.
     * @param enabled Determines whether the channel will be created as enabled or blocked.
     */
    public void createSiteChannel(String origin, boolean enabled) {
        // Channel group must be created before the channel.
        mNotificationManager.createNotificationChannelGroup(
                ChannelDefinitions.getChannelGroup(ChannelDefinitions.CHANNEL_GROUP_ID_SITES));
        mNotificationManager.createNotificationChannel(new Channel(toChannelId(origin), origin,
                enabled ? NotificationManager.IMPORTANCE_DEFAULT
                        : NotificationManager.IMPORTANCE_NONE,
                ChannelDefinitions.CHANNEL_GROUP_ID_SITES));
    }

    /**
     * Deletes the channel associated with this origin.
     */
    public void deleteSiteChannel(String origin) {
        mNotificationManager.deleteNotificationChannel(toChannelId(origin));
    }

    /**
     * Gets the status of the channel associated with this origin.
     * @return ALLOW, BLOCKED, or UNAVAILABLE (if the channel was never created or was deleted).
     */
    public @NotificationChannelStatus int getChannelStatus(String origin) {
        Channel channel = mNotificationManager.getNotificationChannel(toChannelId(origin));
        if (channel == null) return NotificationChannelStatus.UNAVAILABLE;
        return toChannelStatus(channel.getImportance());
    }

    /**
     * Gets an array of active site channels (i.e. they have been created on the notification
     * manager). This includes enabled and blocked channels.
     */
    public SiteChannel[] getSiteChannels() {
        List<Channel> channels = mNotificationManager.getNotificationChannels();
        List<SiteChannel> siteChannels = new ArrayList<>();
        for (Channel channel : channels) {
            if (channel.getGroupId() != null
                    && channel.getGroupId().equals(ChannelDefinitions.CHANNEL_GROUP_ID_SITES)) {
                siteChannels.add(new SiteChannel(
                        toSiteOrigin(channel.getId()), toChannelStatus(channel.getImportance())));
            }
        }
        return siteChannels.toArray(new SiteChannel[siteChannels.size()]);
    }

    /**
     * Converts a site's origin to a canonical channel id for that origin.
     */
    public static String toChannelId(String origin) {
        return ChannelDefinitions.CHANNEL_ID_PREFIX_SITES
                + WebsiteAddress.create(origin).getOrigin();
    }

    /**
     * Converts the channel id of a notification channel to a site origin. This is only valid for
     * site notification channels, i.e. channels with ids beginning with
     * {@link ChannelDefinitions#CHANNEL_ID_PREFIX_SITES}.
     */
    static String toSiteOrigin(String channelId) {
        assert channelId.startsWith(ChannelDefinitions.CHANNEL_ID_PREFIX_SITES);
        return channelId.substring(ChannelDefinitions.CHANNEL_ID_PREFIX_SITES.length());
    }

    /**
     * Converts a notification channel's importance to ENABLED or BLOCKED.
     */
    private static @NotificationChannelStatus int toChannelStatus(int importance) {
        switch (importance) {
            case NotificationManager.IMPORTANCE_NONE:
                return NotificationChannelStatus.BLOCKED;
            default:
                return NotificationChannelStatus.ENABLED;
        }
    }
}
