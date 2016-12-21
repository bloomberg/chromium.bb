// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.PendingIntent;
import android.app.RemoteInput;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.net.URL;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for the Notification Bridge.
 *
 * Web Notifications are only supported on Android JellyBean and beyond.
 */
public class NotificationPlatformBridgeTest extends NotificationTestBase {
    private static final String NOTIFICATION_TEST_PAGE =
            "/chrome/test/data/notifications/android_test.html";
    private static final int TITLE_UPDATE_TIMEOUT_SECONDS = (int) scaleTimeout(5);

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));
    }

    private void waitForTitle(String expectedTitle) throws InterruptedException {
        Tab tab = getActivity().getActivityTab();
        TabTitleObserver titleObserver = new TabTitleObserver(tab, expectedTitle);
        try {
            titleObserver.waitForTitleUpdate(TITLE_UPDATE_TIMEOUT_SECONDS);
        } catch (TimeoutException e) {
            // The title is not as expected, this assertion neatly logs what the difference is.
            assertEquals(expectedTitle, tab.getTitle());
        }
    }

    private InfoBar getInfobarBlocking() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !getInfoBars().isEmpty();
            }
        });
        List<InfoBar> infoBars = getInfoBars();
        assertEquals(1, infoBars.size());
        return infoBars.get(0);
    }

    private void waitForInfobarToClose() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getInfoBars().isEmpty();
            }
        });
        assertEquals(0, getInfoBars().size());
    }

    private void checkThatShowNotificationIsDenied() throws Exception {
        runJavaScriptCodeInCurrentTab("showNotification('MyNotification', {})");
        waitForTitle("TypeError: No notification permission has been granted for this origin.");
        // Ideally we'd wait a little here, but it's hard to wait for things that shouldn't happen.
        assertTrue(getNotificationEntries().isEmpty());
    }

    /**
     * Verifies that notifcations cannot be shown without permission, and that dismissing or denying
     * the infobar works correctly.
     */
    @LargeTest
    @Feature({"Browser", "Notifications"})
    public void testPermissionDenied() throws Exception {
        // Notifications permission should initially be prompt, and showing should fail.
        assertEquals("\"default\"", runJavaScriptCodeInCurrentTab("Notification.permission"));
        checkThatShowNotificationIsDenied();

        // Notification.requestPermission() should show the notifications infobar.
        assertEquals(0, getInfoBars().size());
        runJavaScriptCodeInCurrentTab("Notification.requestPermission(sendToTest)");
        InfoBar infoBar = getInfobarBlocking();

        // Dismissing the infobar should pass prompt to the requestPermission callback.
        assertTrue(InfoBarUtil.clickCloseButton(infoBar));
        waitForInfobarToClose();
        waitForTitle("default"); // See https://crbug.com/434547.

        // Notifications permission should still be prompt.
        assertEquals("\"default\"", runJavaScriptCodeInCurrentTab("Notification.permission"));
        checkThatShowNotificationIsDenied();

        // Notification.requestPermission() should show the notifications infobar again.
        runJavaScriptCodeInCurrentTab("Notification.requestPermission(sendToTest)");
        infoBar = getInfobarBlocking();

        // Denying the infobar should pass denied to the requestPermission callback.
        assertTrue(InfoBarUtil.clickSecondaryButton(infoBar));
        waitForInfobarToClose();
        waitForTitle("denied");

        // This should have caused notifications permission to become denied.
        assertEquals("\"denied\"", runJavaScriptCodeInCurrentTab("Notification.permission"));
        checkThatShowNotificationIsDenied();

        // Reload page to ensure the block is persisted.
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));

        // Notification.requestPermission() should immediately pass denied to the callback without
        // showing an infobar.
        runJavaScriptCodeInCurrentTab("Notification.requestPermission(sendToTest)");
        waitForTitle("denied");
        assertEquals(0, getInfoBars().size());

        // Notifications permission should still be denied.
        assertEquals("\"denied\"", runJavaScriptCodeInCurrentTab("Notification.permission"));
        checkThatShowNotificationIsDenied();
    }

    /**
     * Verifies granting permission via the infobar.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testPermissionGranted() throws Exception {
        // Notifications permission should initially be prompt, and showing should fail.
        assertEquals("\"default\"", runJavaScriptCodeInCurrentTab("Notification.permission"));
        checkThatShowNotificationIsDenied();

        // Notification.requestPermission() should show the notifications infobar.
        assertEquals(0, getInfoBars().size());
        runJavaScriptCodeInCurrentTab("Notification.requestPermission(sendToTest)");
        InfoBar infoBar = getInfobarBlocking();

        // Accepting the infobar should pass granted to the requestPermission callback.
        assertTrue(InfoBarUtil.clickPrimaryButton(infoBar));
        waitForInfobarToClose();
        waitForTitle("granted");

        // Reload page to ensure the grant is persisted.
        loadUrl(getTestServer().getURL(NOTIFICATION_TEST_PAGE));

        // Notifications permission should now be granted, and showing should succeed.
        assertEquals("\"granted\"", runJavaScriptCodeInCurrentTab("Notification.permission"));
        showAndGetNotification("MyNotification", "{}");
    }

    /**
     * Verifies that the intended default properties of a notification will indeed be set on the
     * Notification object that will be send to Android.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    @RetryOnFailure
    public void testDefaultNotificationProperties() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);
        Context context = getInstrumentation().getTargetContext();

        Notification notification = showAndGetNotification("MyNotification", "{ body: 'Hello' }");
        String expectedOrigin =
                UrlFormatter.formatUrlForSecurityDisplay(getOrigin(), false /* showScheme */);

        // Validate the contents of the notification.
        assertEquals("MyNotification", NotificationTestUtil.getExtraTitle(notification));
        assertEquals("Hello", NotificationTestUtil.getExtraText(notification));
        assertEquals(expectedOrigin, NotificationTestUtil.getExtraSubText(notification));

        // Verify that the ticker text contains the notification's title and body.
        String tickerText = notification.tickerText.toString();

        assertTrue(tickerText.contains("MyNotification"));
        assertTrue(tickerText.contains("Hello"));

        // On L+, verify the public version of the notification contains the notification's origin,
        // and that the body text has been replaced.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            assertNotNull(notification.publicVersion);
            assertEquals(context.getString(R.string.notification_hidden_text),
                    NotificationTestUtil.getExtraText(notification.publicVersion));
        }
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
            // On N+, origin should be set as the subtext of the public notification.
            assertEquals(expectedOrigin,
                    NotificationTestUtil.getExtraSubText(notification.publicVersion));
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // On L/M, origin should be the title of the public notification.
            assertEquals(
                    expectedOrigin, NotificationTestUtil.getExtraTitle(notification.publicVersion));
        }

        // Verify that the notification's timestamp is set in the past 60 seconds. This number has
        // no significance, but needs to be high enough to not cause flakiness as it's set by the
        // renderer process on notification creation.
        assertTrue(Math.abs(System.currentTimeMillis() - notification.when) < 60 * 1000);

        assertNotNull(NotificationTestUtil.getLargeIconFromNotification(context, notification));

        // Validate the notification's behavior.
        assertEquals(Notification.DEFAULT_ALL, notification.defaults);
        assertEquals(Notification.PRIORITY_DEFAULT, notification.priority);
    }

    /**
     * Verifies that specifying a notification action with type: 'text' results in a notification
     * with a remote input on the action.
     */
    @CommandLineFlags.Add("enable-experimental-web-platform-features")
    @MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT_WATCH)
    @TargetApi(Build.VERSION_CODES.KITKAT_WATCH) // RemoteInputs were only added in KITKAT_WATCH.
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testShowNotificationWithTextAction() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("MyNotification", "{ "
                        + " actions: [{action: 'myAction', title: 'reply', type: 'text',"
                        + " placeholder: 'hi' }]}");

        // The specified action should be present, as well as a default settings action.
        assertEquals(2, notification.actions.length);

        Notification.Action action = notification.actions[0];
        assertEquals("reply", action.title);
        assertNotNull(notification.actions[0].getRemoteInputs());
        assertEquals(1, action.getRemoteInputs().length);
        assertEquals("hi", action.getRemoteInputs()[0].getLabel());
    }

    /**
     * Verifies that setting a reply on the remote input of a notification action with type 'text'
     * and triggering the action's intent causes the same reply to be received in the subsequent
     * notificationclick event on the service worker.
     */
    @CommandLineFlags.Add("enable-experimental-web-platform-features")
    @MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT_WATCH)
    @TargetApi(Build.VERSION_CODES.KITKAT_WATCH) // RemoteInputs were only added in KITKAT_WATCH.
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testReplyToNotification() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);
        Context context = getInstrumentation().getTargetContext();

        Notification notification = showAndGetNotification("MyNotification", "{ "
                        + " actions: [{action: 'myAction', title: 'reply', type: 'text'}],"
                        + " data: 'ACTION_REPLY'}");

        // Check the action is present with a remote input attached.
        Notification.Action action = notification.actions[0];
        assertEquals("reply", action.title);
        RemoteInput[] remoteInputs = action.getRemoteInputs();
        assertNotNull(remoteInputs);

        // Set a reply using the action's remote input key and send it on the intent.
        sendIntentWithRemoteInput(context, action.actionIntent, remoteInputs,
                remoteInputs[0].getResultKey(), "My Reply" /* reply */);

        // Check reply was received by the service worker (see android_test_worker.js).
        waitForTitle("reply: My Reply");
    }

    /**
     * Verifies that setting an empty reply on the remote input of a notification action with type
     * 'text' and triggering the action's intent causes an empty reply string to be received in the
     * subsequent notificationclick event on the service worker.
     */
    @CommandLineFlags.Add("enable-experimental-web-platform-features")
    @MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT_WATCH)
    @TargetApi(Build.VERSION_CODES.KITKAT_WATCH) // RemoteInputs added in KITKAT_WATCH.
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testReplyToNotificationWithEmptyReply() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);
        Context context = getInstrumentation().getTargetContext();

        Notification notification = showAndGetNotification("MyNotification", "{ "
                        + " actions: [{action: 'myAction', title: 'reply', type: 'text'}],"
                        + " data: 'ACTION_REPLY'}");

        // Check the action is present with a remote input attached.
        Notification.Action action = notification.actions[0];
        assertEquals("reply", action.title);
        RemoteInput[] remoteInputs = action.getRemoteInputs();
        assertNotNull(remoteInputs);

        // Set a reply using the action's remote input key and send it on the intent.
        sendIntentWithRemoteInput(context, action.actionIntent, remoteInputs,
                remoteInputs[0].getResultKey(), "" /* reply */);

        // Check empty reply was received by the service worker (see android_test_worker.js).
        waitForTitle("reply:");
    }

    @TargetApi(Build.VERSION_CODES.KITKAT_WATCH) // RemoteInputs added in KITKAT_WATCH.
    private static void sendIntentWithRemoteInput(Context context, PendingIntent pendingIntent,
            RemoteInput[] remoteInputs, String resultKey, String reply)
            throws PendingIntent.CanceledException {
        Bundle results = new Bundle();
        results.putString(resultKey, reply);
        Intent fillInIntent = new Intent().addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        RemoteInput.addResultsToIntent(remoteInputs, fillInIntent, results);

        // Send the pending intent filled in with the additional information from the new intent.
        pendingIntent.send(context, 0 /* code */, fillInIntent);
    }

    /**
     * Verifies that *not* setting a reply on the remote input of a notification action with type
     * 'text' and triggering the action's intent causes a null reply to be received in the
     * subsequent notificationclick event on the service worker.
     */
    @TargetApi(Build.VERSION_CODES.KITKAT) // Notification.Action.actionIntent added in Android K.
    @CommandLineFlags.Add("enable-experimental-web-platform-features")
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testReplyToNotificationWithNoRemoteInput() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("MyNotification", "{ "
                        + " actions: [{action: 'myAction', title: 'reply', type: 'text'}],"
                        + " data: 'ACTION_REPLY'}");

        assertEquals("reply", notification.actions[0].title);
        notification.actions[0].actionIntent.send();

        // Check reply was received by the service worker (see android_test_worker.js).
        waitForTitle("reply: null");
    }

    /**
     * Verifies that the ONLY_ALERT_ONCE flag is not set when renotify is true.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationRenotifyProperty() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification =
                showAndGetNotification("MyNotification", "{ tag: 'myTag', renotify: true }");

        assertEquals(0, notification.flags & Notification.FLAG_ONLY_ALERT_ONCE);
    }

    /**
     * Verifies that notifications created with the "silent" flag do not inherit system defaults
     * in regards to their sound, vibration and light indicators.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationSilentProperty() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("MyNotification", "{ silent: true }");

        // Zero indicates that no defaults should be inherited from the system.
        assertEquals(0, notification.defaults);
    }

    private void verifyVibrationNotRequestedWhenDisabledInPrefs(String notificationOptions)
            throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        // Disable notification vibration in preferences.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setNotificationsVibrateEnabled(false);
            }
        });

        Notification notification = showAndGetNotification("MyNotification", notificationOptions);

        // Vibration should not be in the defaults.
        assertEquals(
                Notification.DEFAULT_ALL & ~Notification.DEFAULT_VIBRATE, notification.defaults);

        // There should be a custom no-op vibration pattern.
        assertEquals(1, notification.vibrate.length);
        assertEquals(0L, notification.vibrate[0]);
    }

    /**
     * Verifies that when notification vibration is disabled in preferences and no custom pattern is
     * specified, no vibration is requested from the framework.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    @RetryOnFailure
    public void testNotificationVibratePreferenceDisabledDefault() throws Exception {
        verifyVibrationNotRequestedWhenDisabledInPrefs("{}");
    }

    /**
     * Verifies that when notification vibration is disabled in preferences and a custom pattern is
     * specified, no vibration is requested from the framework.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    @RetryOnFailure
    public void testNotificationVibratePreferenceDisabledCustomPattern() throws Exception {
        verifyVibrationNotRequestedWhenDisabledInPrefs("{ vibrate: 42 }");
    }

    /**
     * Verifies that by default the notification vibration preference is enabled, and a custom
     * pattern is passed along.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationVibrateCustomPattern() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        // By default, vibration is enabled in notifications.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertTrue(PrefServiceBridge.getInstance().isNotificationsVibrateEnabled());
            }
        });

        Notification notification = showAndGetNotification("MyNotification", "{ vibrate: 42 }");

        // Vibration should not be in the defaults, a custom pattern was provided.
        assertEquals(
                Notification.DEFAULT_ALL & ~Notification.DEFAULT_VIBRATE, notification.defaults);

        // The custom pattern should have been passed along.
        assertEquals(2, notification.vibrate.length);
        assertEquals(0L, notification.vibrate[0]);
        assertEquals(42L, notification.vibrate[1]);
    }

    /**
     * Verifies that on Android M+, notifications which specify a badge will have that icon
     * fetched and included as the small icon in the notification and public version.
     * If the test target is L or below, verifies the small icon (and public small icon on L) is
     * the expected chrome logo.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testShowNotificationWithBadge() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification =
                showAndGetNotification("MyNotification", "{badge: 'badge.png'}");

        assertEquals("MyNotification", NotificationTestUtil.getExtraTitle(notification));

        Context context = getInstrumentation().getTargetContext();
        Bitmap smallIcon = NotificationTestUtil.getSmallIconFromNotification(context, notification);
        assertNotNull(smallIcon);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // Custom badges are only supported on M+.
            // 1. Check the notification badge.
            URL badgeUrl =
                    new URL(getTestServer().getURL("/chrome/test/data/notifications/badge.png"));
            Bitmap bitmap = BitmapFactory.decodeStream(badgeUrl.openStream());
            Bitmap expected = bitmap.copy(bitmap.getConfig(), true);
            NotificationBuilderBase.applyWhiteOverlayToBitmap(expected);
            assertTrue(expected.sameAs(smallIcon));

            // 2. Check the public notification badge.
            assertNotNull(notification.publicVersion);
            Bitmap publicSmallIcon = NotificationTestUtil.getSmallIconFromNotification(
                    context, notification.publicVersion);
            assertNotNull(publicSmallIcon);
            assertEquals(expected.getWidth(), publicSmallIcon.getWidth());
            assertEquals(expected.getHeight(), publicSmallIcon.getHeight());
            assertTrue(expected.sameAs(publicSmallIcon));
        } else {
            Bitmap expected =
                    BitmapFactory.decodeResource(context.getResources(), R.drawable.ic_chrome);
            assertTrue(expected.sameAs(smallIcon));

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                // Public versions of notifications are only supported on L+.
                assertNotNull(notification.publicVersion);
                Bitmap publicSmallIcon = NotificationTestUtil.getSmallIconFromNotification(
                        context, notification.publicVersion);
                assertTrue(expected.sameAs(publicSmallIcon));
            }
        }
    }

    /**
     * Verifies that notifications which specify an icon will have that icon fetched, converted into
     * a Bitmap and included as the large icon in the notification.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    @RetryOnFailure
    public void testShowNotificationWithIcon() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("MyNotification", "{icon: 'red.png'}");

        assertEquals("MyNotification", NotificationTestUtil.getExtraTitle(notification));

        Context context = getInstrumentation().getTargetContext();
        Bitmap largeIcon = NotificationTestUtil.getLargeIconFromNotification(context, notification);
        assertNotNull(largeIcon);
        assertEquals(Color.RED, largeIcon.getPixel(0, 0));
    }

    /**
     * Verifies that notifications which don't specify an icon will get an automatically generated
     * icon based on their origin. The size of these icons are dependent on the resolution of the
     * device the test is being ran on, so we create it again in order to compare.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    @RetryOnFailure
    public void testShowNotificationWithoutIcon() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("NoIconNotification", "{}");

        assertEquals("NoIconNotification", NotificationTestUtil.getExtraTitle(notification));

        Context context = getInstrumentation().getTargetContext();
        assertNotNull(NotificationTestUtil.getLargeIconFromNotification(context, notification));

        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        assertNotNull(notificationBridge);

        // Create a second rounded icon for the test's origin, and compare its dimensions against
        // those of the icon associated to the notification itself.
        RoundedIconGenerator generator =
                NotificationBuilderBase.createIconGenerator(context.getResources());

        Bitmap generatedIcon = generator.generateIconForUrl(getOrigin());
        assertNotNull(generatedIcon);
        assertTrue(generatedIcon.sameAs(
                NotificationTestUtil.getLargeIconFromNotification(context, notification)));
    }

    /*
     * Verifies that starting the PendingIntent stored as the notification's content intent will
     * start up the associated Service Worker, where the JavaScript code will close the notification
     * by calling event.notification.close().
     */
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @RetryOnFailure
    public void testNotificationContentIntentClosesNotification() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = showAndGetNotification("MyNotification", "{}");

        // Sending the PendingIntent resembles activating the notification.
        assertNotNull(notification.contentIntent);
        notification.contentIntent.send();

        // The Service Worker will close the notification upon receiving the notificationclick
        // event. This will eventually bubble up to a call to cancel() in the NotificationManager.
        waitForNotificationManagerMutation();
        assertTrue(getNotificationEntries().isEmpty());
    }

    /**
     * Verifies that starting the PendingIntent stored as the notification's content intent will
     * start up the associated Service Worker, where the JavaScript code will create a new tab for
     * displaying the notification's event to the user.
     */
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @RetryOnFailure
    public void testNotificationContentIntentCreatesTab() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        assertEquals("Expected the notification test page to be the sole tab in the current model",
                1, getActivity().getCurrentTabModel().getCount());

        Notification notification =
                showAndGetNotification("MyNotification", "{ data: 'ACTION_CREATE_TAB' }");

        // Sending the PendingIntent resembles activating the notification.
        assertNotNull(notification.contentIntent);
        notification.contentIntent.send();

        // The Service Worker, upon receiving the notificationclick event, will create a new tab
        // after which it closes the notification.
        waitForNotificationManagerMutation();
        assertTrue(getNotificationEntries().isEmpty());

        CriteriaHelper.pollInstrumentationThread(new Criteria("Expected a new tab to be created") {
            @Override
            public boolean isSatisfied() {
                return 2 == getActivity().getCurrentTabModel().getCount();
            }
        });
    }

    /**
     * Verifies that creating a notification with an associated "tag" will cause any previous
     * notification with the same tag to be dismissed prior to being shown.
     */
    @SuppressFBWarnings("DLS_DEAD_LOCAL_STORE")
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationTagReplacement() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        runJavaScriptCodeInCurrentTab("showNotification('MyNotification', {tag: 'myTag'});");
        waitForNotificationManagerMutation();
        List<NotificationEntry> notifications = getNotificationEntries();
        String tag = notifications.get(0).tag;
        int id = notifications.get(0).id;

        runJavaScriptCodeInCurrentTab("showNotification('SecondNotification', {tag: 'myTag'});");
        waitForNotificationManagerMutation();

        // Verify that the notification was successfully replaced.
        notifications = getNotificationEntries();
        assertEquals(1, notifications.size());
        assertEquals("SecondNotification",
                NotificationTestUtil.getExtraTitle(notifications.get(0).notification));

        // Verify that for replaced notifications their tag was the same.
        assertEquals(tag, notifications.get(0).tag);

        // Verify that as always, the same integer is used, also for replaced notifications.
        assertEquals(id, notifications.get(0).id);
        assertEquals(NotificationPlatformBridge.PLATFORM_ID, notifications.get(0).id);
    }

    /**
     * Verifies that multiple notifications without a tag can be opened and closed without
     * affecting eachother.
     */
    @LargeTest
    @Feature({"Browser", "Notifications"})
    public void testShowAndCloseMultipleNotifications() throws Exception {
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        // Open the first notification and verify it is displayed.
        runJavaScriptCodeInCurrentTab("showNotification('One');");
        waitForNotificationManagerMutation();
        List<NotificationEntry> notifications = getNotificationEntries();
        assertEquals(1, notifications.size());
        Notification notificationOne = notifications.get(0).notification;
        assertEquals("One", NotificationTestUtil.getExtraTitle(notificationOne));

        // Open the second notification and verify it is displayed.
        runJavaScriptCodeInCurrentTab("showNotification('Two');");
        waitForNotificationManagerMutation();
        notifications = getNotificationEntries();
        assertEquals(2, notifications.size());
        Notification notificationTwo = notifications.get(1).notification;
        assertEquals("Two", NotificationTestUtil.getExtraTitle(notificationTwo));

        // The same integer id is always used as it is not needed for uniqueness, we rely on the tag
        // for uniqueness when the replacement behavior is not needed.
        assertEquals(NotificationPlatformBridge.PLATFORM_ID, notifications.get(0).id);
        assertEquals(NotificationPlatformBridge.PLATFORM_ID, notifications.get(1).id);

        // As these notifications were not meant to replace eachother, they must not have the same
        // tag internally.
        assertFalse(notifications.get(0).tag.equals(notifications.get(1).tag));

        // Verify that the PendingIntent for content and delete is different for each notification.
        assertFalse(notificationOne.contentIntent.equals(notificationTwo.contentIntent));
        assertFalse(notificationOne.deleteIntent.equals(notificationTwo.deleteIntent));

        // Close the first notification and verify that only the second remains.
        // Sending the content intent resembles touching the notification. In response tho this the
        // notificationclick event is fired. The test service worker will close the notification
        // upon receiving the event.
        notificationOne.contentIntent.send();
        waitForNotificationManagerMutation();
        notifications = getNotificationEntries();
        assertEquals(1, notifications.size());
        assertEquals("Two", NotificationTestUtil.getExtraTitle(notifications.get(0).notification));

        // Close the last notification and verify that none remain.
        notifications.get(0).notification.contentIntent.send();
        waitForNotificationManagerMutation();
        assertTrue(getNotificationEntries().isEmpty());
    }
}
