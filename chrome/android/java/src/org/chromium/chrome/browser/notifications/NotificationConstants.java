// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

/**
 * Constants used in more than a single Notification class, e.g. intents and extra names.
 */
public class NotificationConstants {
    // These actions have to be synchronized with the receiver defined in AndroidManifest.xml.
    public static final String ACTION_CLICK_NOTIFICATION =
            "org.chromium.chrome.browser.notifications.CLICK_NOTIFICATION";
    public static final String ACTION_CLOSE_NOTIFICATION =
            "org.chromium.chrome.browser.notifications.CLOSE_NOTIFICATION";

    public static final String EXTRA_NOTIFICATION_ID = "notification_id";
    public static final String EXTRA_NOTIFICATION_TAG = "notification_tag";

    // TODO(peter): Remove these extras once Notifications are powered by a database on the
    // native side, that contains all the additional information.
    public static final String EXTRA_NOTIFICATION_PLATFORM_ID = "notification_platform_id";
    public static final String EXTRA_NOTIFICATION_DATA = "notification_data";

    /**
     * Unique identifier for a single sync notification. Since the notification ID is reused,
     * old notifications will be overwritten.
     */
    public static final int NOTIFICATION_ID_SYNC = 1;
    /**
     * Unique identifier for the "Signed in to Chrome" notification.
     */
    public static final int NOTIFICATION_ID_SIGNED_IN = 2;

    /**
     * Separator used to separate the notification origin from additional data such as the
     * developer specified tag.
     */
    public static final String NOTIFICATION_TAG_SEPARATOR = ";";
}
