// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import org.chromium.base.BuildInfo;
import org.chromium.base.annotations.CalledByNative;

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
     * @return true if the channel was successfully created, false otherwise.
     */
    @CalledByNative
    static boolean createChannel(String origin, boolean enabled) {
        // TODO(crbug.com/700377) Actually construct a channel.
        return false;
    }

    @CalledByNative
    static @NotificationChannelStatus int getChannelStatus(String origin) {
        // TODO(crbug.com/700377) Actually check channel status.
        return NotificationChannelStatus.UNAVAILABLE;
    }

    @CalledByNative
    static SiteChannel[] getSiteChannels() {
        // TODO(crbug.com/700377) Actually get site channels.
        return new SiteChannel[] {};
    }

    @CalledByNative
    static void deleteChannel(String origin) {
        // TODO(crbug.com/700377) Actually delete channel.
    }

    static class SiteChannel {
        private final String mOrigin;
        private final @NotificationChannelStatus int mStatus;

        private SiteChannel(String origin, @NotificationChannelStatus int status) {
            mOrigin = origin;
            mStatus = status;
        }

        @CalledByNative("SiteChannel")
        private String getOrigin() {
            return mOrigin;
        }

        @CalledByNative("SiteChannel")
        private @NotificationChannelStatus int getStatus() {
            return mStatus;
        }
    }
}
