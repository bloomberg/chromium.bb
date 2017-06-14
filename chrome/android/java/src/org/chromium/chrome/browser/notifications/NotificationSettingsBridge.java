// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import org.chromium.base.BuildInfo;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;

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
     * Creates a notification channel for the given origin.
     * @param origin The site origin to be used as the channel name.
     * @param enabled True if the channel should be initially enabled, false if
     *                it should start off as blocked.
     */
    @CalledByNative
    static void createChannel(String origin, boolean enabled) {
        SiteChannelsManager.getInstance().createSiteChannel(origin, enabled);
    }

    @CalledByNative
    static @NotificationChannelStatus int getChannelStatus(String origin) {
        return SiteChannelsManager.getInstance().getChannelStatus(origin);
    }

    @CalledByNative
    static SiteChannel[] getSiteChannels() {
        return SiteChannelsManager.getInstance().getSiteChannels();
    }

    @CalledByNative
    static void deleteChannel(String origin) {
        SiteChannelsManager.getInstance().deleteSiteChannel(origin);
    }

    /**
     * Helper type for passing site channel objects across the JNI.
     */
    public static class SiteChannel {
        private final String mOrigin;
        private final @NotificationChannelStatus int mStatus;

        public SiteChannel(String origin, @NotificationChannelStatus int status) {
            mOrigin = origin;
            mStatus = status;
        }

        @CalledByNative("SiteChannel")
        public String getOrigin() {
            return mOrigin;
        }

        @CalledByNative("SiteChannel")
        public @NotificationChannelStatus int getStatus() {
            return mStatus;
        }
    }
}
