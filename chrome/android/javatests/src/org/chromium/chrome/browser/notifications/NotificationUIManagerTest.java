// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.graphics.Bitmap;
import android.os.Build;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.util.SparseArray;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.PushNotificationInfo;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for the Notification UI Manager implementation on Android.
 *
 * Web Notifications are only supported on Android JellyBean and beyond.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.JELLY_BEAN)
public class NotificationUIManagerTest extends ChromeShellTestBase {
    private static final String NOTIFICATION_TEST_PAGE =
            TestHttpServerClient.getUrl("chrome/test/data/notifications/android_test.html");

    private MockNotificationManagerProxy mMockNotificationManager;

    /**
     * Returns the origin of the HTTP server the test is being ran on.
     */
    private static String getOrigin() {
        return TestHttpServerClient.getUrl("");
    }

    /**
     * Sets the permission to use Web Notifications for the test HTTP server's origin to |setting|.
     */
    private void setNotificationContentSettingForCurrentOrigin(final ContentSetting setting)
            throws InterruptedException, TimeoutException {
        final String origin = getOrigin();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // The notification content setting does not consider the embedder origin.
                PushNotificationInfo pushNotificationInfo = new PushNotificationInfo(origin, "");
                pushNotificationInfo.setContentSetting(setting);
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
     * Runs the Javascript |code| in the current tab, and waits for the result to be available.
     *
     * @param code The JavaScript code to execute in the current tab.
     * @return A JSON-formatted string with the result of executing the |code|.
     */
    private String runJavaScriptCodeInCurrentTab(String code) throws InterruptedException,
                                                                     TimeoutException {
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getActivity().getActiveContentViewCore().getWebContents(), code);
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
    private Notification showAndGetNotification(String title, String options) throws Exception {
        runJavaScriptCodeInCurrentTab("showNotification(\"" + title + "\", " + options + ");");
        assertTrue(waitForNotificationManagerMutation());

        SparseArray<Notification> notifications = mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());

        return notifications.valueAt(0);
    }

    /**
     * Waits for a mutation to occur in the mocked notification manager. This indicates that Chrome
     * called into Android to notify or cancel a notification.
     *
     * @return Whether the wait was successful.
     */
    private boolean waitForNotificationManagerMutation() throws Exception {
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mMockNotificationManager.getMutationCountAndDecrement() > 0;
            }
        });
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mMockNotificationManager = new MockNotificationManagerProxy();
        NotificationUIManager.overrideNotificationManagerForTesting(mMockNotificationManager);

        launchChromeShellWithUrl(NOTIFICATION_TEST_PAGE);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
    }

    @Override
    protected void tearDown() throws Exception {
        NotificationUIManager.overrideNotificationManagerForTesting(null);

        super.tearDown();
    }

    /**
     * Verifies that the intended default properties of a notification will indeed be set on the
     * Notification object that will be send to Android.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testDefaultNotificationProperties() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("MyNotification", "{ body: 'Hello' }");

        // Validate the contents of the notification.
        assertEquals("MyNotification", notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals("Hello", notification.extras.getString(Notification.EXTRA_TEXT));
        assertEquals(getOrigin(), notification.extras.getString(Notification.EXTRA_SUB_TEXT));

        // Validate the appearance style of the notification. The EXTRA_TEMPLATE was inroduced
        // in Android Lollipop, we cannot verify this in earlier versions.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            assertEquals("android.app.Notification$BigTextStyle",
                    notification.extras.getString(Notification.EXTRA_TEMPLATE));
        }

        assertNotNull(notification.largeIcon);

        // Validate the notification's behavior.
        assertEquals(Notification.DEFAULT_ALL, notification.defaults);
        assertEquals(Notification.PRIORITY_DEFAULT, notification.priority);
    }

    /**
     * Verifies that notifications which specify an icon will have that icon fetched, converted into
     * a Bitmap and included as the large icon in the notification.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testShowNotificationWithIcon() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("MyNotification", "{icon: 'icon.png'}");

        assertEquals("MyNotification", notification.extras.getString(Notification.EXTRA_TITLE));
        assertNotNull(notification.largeIcon);

        // These are the dimensions of //chrome/test/data/notifications/icon.png at 1x scale.
        assertEquals(100, notification.largeIcon.getWidth());
        assertEquals(100, notification.largeIcon.getHeight());
    }

    /**
     * Verifies that notifications which don't specify an icon will get an automatically generated
     * icon based on their origin. The size of these icons are dependent on the resolution of the
     * device the test is being ran on, so we create it again in order to compare.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testShowNotificationWithoutIcon() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("NoIconNotification", "{}");

        assertEquals("NoIconNotification", notification.extras.getString(Notification.EXTRA_TITLE));
        assertNotNull(notification.largeIcon);

        // Create a second rounded icon for the test's origin, and compare its dimensions against
        // those of the icon associated to the notification itself.
        RoundedIconGenerator generator = NotificationUIManager.createRoundedIconGenerator(
                getActivity().getApplicationContext());
        assertNotNull(generator);

        Bitmap generatedIcon = generator.generateIconForUrl(getOrigin());
        assertNotNull(generatedIcon);

        assertEquals(generatedIcon.getWidth(), notification.largeIcon.getWidth());
        assertEquals(generatedIcon.getHeight(), notification.largeIcon.getHeight());
    }

    /*
     * Verifies that starting the PendingIntent stored as the notification's delete intent will
     * close the notification.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationDeleteIntentClosesNotification() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("MyNotification", "{}");
        assertEquals(1, mMockNotificationManager.getNotifications().size());

        // Sending the PendingIntent simulates dismissing (swiping away) the notification.
        assertNotNull(notification.deleteIntent);
        notification.deleteIntent.send();
        // The deleteIntent should trigger a call to cancel() in the NotificationManager.
        assertTrue(waitForNotificationManagerMutation());

        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    /*
     * Verifies that starting the PendingIntent stored as the notification's content intent will
     * start up the associated Service Worker, where the JavaScript code will close the notification
     * by calling event.notification.close().
     */
    @LargeTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationContentIntentClosesNotification() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("MyNotification", "{}");

        // Sending the PendingIntent resembles activating the notification.
        assertNotNull(notification.contentIntent);
        notification.contentIntent.send();

        // The Service Worker will close the notification upon receiving the notificationclick
        // event. This will eventually bubble up to a call to cancel() in the NotificationManager.
        assertTrue(waitForNotificationManagerMutation());

        SparseArray<Notification> notifications = mMockNotificationManager.getNotifications();
        assertEquals(0, notifications.size());
    }

    /**
     * Verifies that creating a notification with an associated "tag" will cause any previous
     * notification with the same tag to be dismissed prior to being shown.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationTagReplacement() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("MyNotification", "{tag: 'myTag'}");

        // Show the second notification with the same tag. We can't use showAndGetNotification for
        // this purpose since a second notification would be shown.
        runJavaScriptCodeInCurrentTab("showNotification('SecondNotification', {tag: 'myTag'});");
        assertTrue(waitForNotificationManagerMutation());

        // Verify that the notification was successfully replaced.
        SparseArray<Notification> notifications = mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());
        assertEquals("SecondNotification",
                notifications.valueAt(0).extras.getString(Notification.EXTRA_TITLE));
    }
}
