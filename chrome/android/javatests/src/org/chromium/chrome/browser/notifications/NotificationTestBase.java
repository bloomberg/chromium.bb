// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.app.Notification;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.NotificationInfo;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Base class for instrumentation tests using Web Notifications on Android.
 *
 * Web Notifications are only supported on Android JellyBean and beyond.
 */
public class NotificationTestBase extends ChromeTabbedActivityTestBase {
    /** The maximum time to wait for a criteria to become valid. */
    private static final long MAX_TIME_TO_POLL_MS = scaleTimeout(6000);

    /** The polling interval to wait between checking for a satisfied criteria. */
    private static final long POLLING_INTERVAL_MS = 50;

    private MockNotificationManagerProxy mMockNotificationManager;
    private EmbeddedTestServer mTestServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
    }

    /** Returns the test server. */
    protected EmbeddedTestServer getTestServer() {
        return mTestServer;
    }

    /**
     * Returns the origin of the HTTP server the test is being ran on.
     */
    protected String getOrigin() {
        return mTestServer.getURL("/");
    }

    /**
     * Sets the permission to use Web Notifications for the test HTTP server's origin to |setting|.
     */
    protected void setNotificationContentSettingForCurrentOrigin(final ContentSetting setting)
            throws InterruptedException, TimeoutException {
        final String origin = getOrigin();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // The notification content setting does not consider the embedder origin.
                NotificationInfo notificationInfo = new NotificationInfo(origin, "", false);
                notificationInfo.setContentSetting(setting);
            }
        });

        String permission = runJavaScriptCodeInCurrentTab("Notification.permission");
        if (setting == ContentSetting.ALLOW) {
            assertEquals("\"granted\"", permission);
        } else if (setting == ContentSetting.BLOCK) {
            assertEquals("\"denied\"", permission);
        } else {
            assertEquals("\"default\"", permission);
        }
    }

    /**
     * Shows a notification with |title| and |options|, waits until it has been displayed and then
     * returns the Notification object to the caller. Requires that only a single notification is
     * being displayed in the notification manager.
     *
     * @param title Title of the Web Notification to show.
     * @param options Optional map of options to include when showing the notification.
     * @return The Android Notification object, as shown in the framework.
     */
    protected Notification showAndGetNotification(String title, String options)
            throws InterruptedException, TimeoutException {
        runJavaScriptCodeInCurrentTab("showNotification(\"" + title + "\", " + options + ");");
        return waitForNotification().notification;
    }

    /**
     * Waits until a notification has been displayed and then returns a NotificationEntry object to
     * the caller. Requires that only a single notification is displayed.
     *
     * @return The NotificationEntry object tracked by the MockNotificationManagerProxy.
     */
    protected NotificationEntry waitForNotification() throws InterruptedException {
        waitForNotificationManagerMutation();
        List<NotificationEntry> notifications = getNotificationEntries();
        assertEquals(1, notifications.size());
        return notifications.get(0);
    }

    protected List<NotificationEntry> getNotificationEntries() {
        return mMockNotificationManager.getNotifications();
    }

    /**
     * Waits for a mutation to occur in the mocked notification manager. This indicates that Chrome
     * called into Android to notify or cancel a notification.
     */
    protected void waitForNotificationManagerMutation() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mMockNotificationManager.getMutationCountAndDecrement() > 0;
            }
        }, MAX_TIME_TO_POLL_MS, POLLING_INTERVAL_MS);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        // The NotificationPlatformBridge must be overriden prior to the browser process starting.
        mMockNotificationManager = new MockNotificationManagerProxy();
        NotificationPlatformBridge.overrideNotificationManagerForTesting(mMockNotificationManager);

        startMainActivityFromLauncher();
    }

    @Override
    protected void tearDown() throws Exception {
        NotificationPlatformBridge.overrideNotificationManagerForTesting(null);
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }
}
