// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.junit.Assert.assertThat;

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
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.engagement.SiteEngagementService;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.net.URL;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for the Notification Bridge.
 *
 * Web Notifications are only supported on Android JellyBean and beyond.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NotificationPlatformBridgeTest {
    @Rule
    public NotificationTestRule mNotificationTestRule = new NotificationTestRule();

    private static final String NOTIFICATION_TEST_PAGE =
            "/chrome/test/data/notifications/android_test.html";
    private static final int TITLE_UPDATE_TIMEOUT_SECONDS = (int) scaleTimeout(5);

    @Before
    public void setUp() throws Exception {
        SiteEngagementService.setParamValuesForTesting();
        mNotificationTestRule.loadUrl(
                mNotificationTestRule.getTestServer().getURL(NOTIFICATION_TEST_PAGE));
    }

    @SuppressWarnings("MissingFail")
    private void waitForTitle(String expectedTitle) throws InterruptedException {
        Tab tab = mNotificationTestRule.getActivity().getActivityTab();
        TabTitleObserver titleObserver = new TabTitleObserver(tab, expectedTitle);
        try {
            titleObserver.waitForTitleUpdate(TITLE_UPDATE_TIMEOUT_SECONDS);

        } catch (TimeoutException e) {
            // The title is not as expected, this assertion neatly logs what the difference is.
            Assert.assertEquals(expectedTitle, tab.getTitle());
        }
    }

    private InfoBar getInfobarBlocking() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !mNotificationTestRule.getInfoBars().isEmpty();
            }
        });
        List<InfoBar> infoBars = mNotificationTestRule.getInfoBars();
        Assert.assertEquals(1, infoBars.size());
        return infoBars.get(0);
    }

    private void waitForInfobarToClose() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mNotificationTestRule.getInfoBars().isEmpty();
            }
        });
        Assert.assertEquals(0, mNotificationTestRule.getInfoBars().size());
    }

    private void checkThatShowNotificationIsDenied() throws Exception {
        mNotificationTestRule.runJavaScriptCodeInCurrentTab(
                "showNotification('MyNotification', {})");
        waitForTitle("TypeError: No notification permission has been granted for this origin.");
        // Ideally we'd wait a little here, but it's hard to wait for things that shouldn't happen.
        Assert.assertTrue(mNotificationTestRule.getNotificationEntries().isEmpty());
    }

    private double getEngagementScoreBlocking() {
        try {
            return ThreadUtils.runOnUiThreadBlocking(new Callable<Double>() {
                @Override
                public Double call() throws Exception {
                    return SiteEngagementService.getForProfile(Profile.getLastUsedProfile())
                            .getScore(mNotificationTestRule.getOrigin());
                }
            });
        } catch (ExecutionException ex) {
            assert false : "Unexpected ExecutionException";
        }
        return 0.0;
    }

    /**
     * Verifies that notifcations cannot be shown without permission, and that dismissing or denying
     * the infobar works correctly.
     */
    //@LargeTest
    //@Feature({"Browser", "Notifications"})
    // crbug.com/707528
    @Test
    @DisabledTest
    public void testPermissionDenied() throws Exception {
        // Notifications permission should initially be prompt, and showing should fail.
        Assert.assertEquals("\"default\"",
                mNotificationTestRule.runJavaScriptCodeInCurrentTab("Notification.permission"));
        checkThatShowNotificationIsDenied();

        // Notification.requestPermission() should show the notifications infobar.
        Assert.assertEquals(0, mNotificationTestRule.getInfoBars().size());
        mNotificationTestRule.runJavaScriptCodeInCurrentTab(
                "Notification.requestPermission(sendToTest)");
        InfoBar infoBar = getInfobarBlocking();

        // Dismissing the infobar should pass prompt to the requestPermission callback.
        Assert.assertTrue(InfoBarUtil.clickCloseButton(infoBar));
        waitForInfobarToClose();
        waitForTitle("default"); // See https://crbug.com/434547.

        // Notifications permission should still be prompt.
        Assert.assertEquals("\"default\"",
                mNotificationTestRule.runJavaScriptCodeInCurrentTab("Notification.permission"));
        checkThatShowNotificationIsDenied();

        // Notification.requestPermission() should show the notifications infobar again.
        mNotificationTestRule.runJavaScriptCodeInCurrentTab(
                "Notification.requestPermission(sendToTest)");
        infoBar = getInfobarBlocking();

        // Denying the infobar should pass denied to the requestPermission callback.
        Assert.assertTrue(InfoBarUtil.clickSecondaryButton(infoBar));
        waitForInfobarToClose();
        waitForTitle("denied");

        // This should have caused notifications permission to become denied.
        Assert.assertEquals("\"denied\"",
                mNotificationTestRule.runJavaScriptCodeInCurrentTab("Notification.permission"));
        checkThatShowNotificationIsDenied();

        // Reload page to ensure the block is persisted.
        mNotificationTestRule.loadUrl(
                mNotificationTestRule.getTestServer().getURL(NOTIFICATION_TEST_PAGE));

        // Notification.requestPermission() should immediately pass denied to the callback without
        // showing an infobar.
        mNotificationTestRule.runJavaScriptCodeInCurrentTab(
                "Notification.requestPermission(sendToTest)");
        waitForTitle("denied");
        Assert.assertEquals(0, mNotificationTestRule.getInfoBars().size());

        // Notifications permission should still be denied.
        Assert.assertEquals("\"denied\"",
                mNotificationTestRule.runJavaScriptCodeInCurrentTab("Notification.permission"));
        checkThatShowNotificationIsDenied();
    }

    /**
     * Verifies granting permission via the infobar.
     */
    //@MediumTest
    //@Feature({"Browser", "Notifications"})
    // crbug.com/707528
    @Test
    @DisabledTest
    public void testPermissionGranted() throws Exception {
        // Notifications permission should initially be prompt, and showing should fail.
        Assert.assertEquals("\"default\"",
                mNotificationTestRule.runJavaScriptCodeInCurrentTab("Notification.permission"));
        checkThatShowNotificationIsDenied();

        // Notification.requestPermission() should show the notifications infobar.
        Assert.assertEquals(0, mNotificationTestRule.getInfoBars().size());
        mNotificationTestRule.runJavaScriptCodeInCurrentTab(
                "Notification.requestPermission(sendToTest)");
        InfoBar infoBar = getInfobarBlocking();

        // Accepting the infobar should pass granted to the requestPermission callback.
        Assert.assertTrue(InfoBarUtil.clickPrimaryButton(infoBar));
        waitForInfobarToClose();
        waitForTitle("granted");

        // Reload page to ensure the grant is persisted.
        mNotificationTestRule.loadUrl(
                mNotificationTestRule.getTestServer().getURL(NOTIFICATION_TEST_PAGE));

        // Notifications permission should now be granted, and showing should succeed.
        Assert.assertEquals("\"granted\"",
                mNotificationTestRule.runJavaScriptCodeInCurrentTab("Notification.permission"));
        mNotificationTestRule.showAndGetNotification("MyNotification", "{}");
    }

    /**
     * Verifies that the intended default properties of a notification will indeed be set on the
     * Notification object that will be send to Android.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    @RetryOnFailure
    public void testDefaultNotificationProperties() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);
        Context context = InstrumentationRegistry.getTargetContext();

        Notification notification =
                mNotificationTestRule.showAndGetNotification("MyNotification", "{ body: 'Hello' }");
        String expectedOrigin = UrlFormatter.formatUrlForSecurityDisplayOmitScheme(
                mNotificationTestRule.getOrigin());

        // Validate the contents of the notification.
        Assert.assertEquals("MyNotification", NotificationTestUtil.getExtraTitle(notification));
        Assert.assertEquals("Hello", NotificationTestUtil.getExtraText(notification));
        Assert.assertEquals(expectedOrigin, NotificationTestUtil.getExtraSubText(notification));

        // Verify that the ticker text contains the notification's title and body.
        String tickerText = notification.tickerText.toString();

        Assert.assertTrue(tickerText.contains("MyNotification"));
        Assert.assertTrue(tickerText.contains("Hello"));

        // On L+, verify the public version of the notification contains the notification's origin,
        // and that the body text has been replaced.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            Assert.assertNotNull(notification.publicVersion);
            Assert.assertEquals(context.getString(R.string.notification_hidden_text),
                    NotificationTestUtil.getExtraText(notification.publicVersion));
        }
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
            // On N+, origin should be set as the subtext of the public notification.
            Assert.assertEquals(expectedOrigin,
                    NotificationTestUtil.getExtraSubText(notification.publicVersion));
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // On L/M, origin should be the title of the public notification.
            Assert.assertEquals(
                    expectedOrigin, NotificationTestUtil.getExtraTitle(notification.publicVersion));
        }

        // Verify that the notification's timestamp is set in the past 60 seconds. This number has
        // no significance, but needs to be high enough to not cause flakiness as it's set by the
        // renderer process on notification creation.
        Assert.assertTrue(Math.abs(System.currentTimeMillis() - notification.when) < 60 * 1000);

        boolean timestampIsShown = NotificationTestUtil.getExtraShownWhen(notification);
        Assert.assertTrue("Timestamp should be shown", timestampIsShown);

        Assert.assertNotNull(
                NotificationTestUtil.getLargeIconFromNotification(context, notification));

        // Validate the notification's behavior.
        Assert.assertEquals(Notification.DEFAULT_ALL, notification.defaults);
        Assert.assertEquals(Notification.PRIORITY_DEFAULT, notification.priority);
    }

    /**
     * Verifies that specifying a notification action with type: 'text' results in a notification
     * with a remote input on the action.
     */
    @Test
    @CommandLineFlags.Add("enable-experimental-web-platform-features")
    @MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT_WATCH)
    @TargetApi(Build.VERSION_CODES.KITKAT_WATCH) // RemoteInputs were only added in KITKAT_WATCH.
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testShowNotificationWithTextAction() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = mNotificationTestRule.showAndGetNotification("MyNotification",
                "{ "
                        + " actions: [{action: 'myAction', title: 'reply', type: 'text',"
                        + " placeholder: 'hi' }]}");

        // The specified action should be present, as well as a default settings action.
        Assert.assertEquals(2, notification.actions.length);

        Notification.Action action = notification.actions[0];
        Assert.assertEquals("reply", action.title);
        Assert.assertNotNull(notification.actions[0].getRemoteInputs());
        Assert.assertEquals(1, action.getRemoteInputs().length);
        Assert.assertEquals("hi", action.getRemoteInputs()[0].getLabel());
    }

    /**
     * Verifies that setting a reply on the remote input of a notification action with type 'text'
     * and triggering the action's intent causes the same reply to be received in the subsequent
     * notificationclick event on the service worker. Verifies that site engagement is incremented
     * appropriately.
     */
    @Test
    @CommandLineFlags.Add("enable-experimental-web-platform-features")
    @MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT_WATCH)
    @TargetApi(Build.VERSION_CODES.KITKAT_WATCH) // RemoteInputs were only added in KITKAT_WATCH.
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testReplyToNotification() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);
        Context context = InstrumentationRegistry.getTargetContext();

        UserActionTester actionTester = new UserActionTester();

        // +0.5 engagement from navigating to the test page.
        Assert.assertEquals(0.5, getEngagementScoreBlocking(), 0);
        Notification notification = mNotificationTestRule.showAndGetNotification("MyNotification",
                "{ "
                        + " actions: [{action: 'myAction', title: 'reply', type: 'text'}],"
                        + " data: 'ACTION_REPLY'}");

        // Check the action is present with a remote input attached.
        Notification.Action action = notification.actions[0];
        Assert.assertEquals("reply", action.title);
        RemoteInput[] remoteInputs = action.getRemoteInputs();
        Assert.assertNotNull(remoteInputs);

        // Set a reply using the action's remote input key and send it on the intent.
        Helper.sendIntentWithRemoteInput(context, action.actionIntent, remoteInputs,
                remoteInputs[0].getResultKey(), "My Reply" /* reply */);

        // Check reply was received by the service worker (see android_test_worker.js).
        // Expect +1 engagement from interacting with the notification.
        waitForTitle("reply: My Reply");
        Assert.assertEquals(1.5, getEngagementScoreBlocking(), 0);

        // Replies are always delivered to an action button.
        assertThat(actionTester.toString(), getNotificationActions(actionTester),
                Matchers.contains("Notifications.Persistent.Shown",
                        "Notifications.Persistent.ClickedActionButton"));
    }

    /**
     * Verifies that setting an empty reply on the remote input of a notification action with type
     * 'text' and triggering the action's intent causes an empty reply string to be received in the
     * subsequent notificationclick event on the service worker. Verifies that site engagement is
     * incremented appropriately.
     */
    @Test
    @CommandLineFlags.Add("enable-experimental-web-platform-features")
    @MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT_WATCH)
    @TargetApi(Build.VERSION_CODES.KITKAT_WATCH) // RemoteInputs added in KITKAT_WATCH.
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testReplyToNotificationWithEmptyReply() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);
        Context context = InstrumentationRegistry.getTargetContext();

        // +0.5 engagement from navigating to the test page.
        Assert.assertEquals(0.5, getEngagementScoreBlocking(), 0);
        Notification notification = mNotificationTestRule.showAndGetNotification("MyNotification",
                "{ "
                        + " actions: [{action: 'myAction', title: 'reply', type: 'text'}],"
                        + " data: 'ACTION_REPLY'}");

        // Check the action is present with a remote input attached.
        Notification.Action action = notification.actions[0];
        Assert.assertEquals("reply", action.title);
        RemoteInput[] remoteInputs = action.getRemoteInputs();
        Assert.assertNotNull(remoteInputs);

        // Set a reply using the action's remote input key and send it on the intent.
        Helper.sendIntentWithRemoteInput(context, action.actionIntent, remoteInputs,
                remoteInputs[0].getResultKey(), "" /* reply */);

        // Check empty reply was received by the service worker (see android_test_worker.js).
        // Expect +1 engagement from interacting with the notification.
        waitForTitle("reply:");
        Assert.assertEquals(1.5, getEngagementScoreBlocking(), 0);
    }

    //TODO(yolandyan): remove this after supporting SdkSuppress annotation
    //Currently JUnit4 reflection would look for all test methods in a Test class, and
    //this test uses RemoteInput as input, this would cause NoClassDefFoundError
    //in lower than L devices
    private static class Helper {
        @TargetApi(Build.VERSION_CODES.KITKAT_WATCH) // RemoteInputs added in KITKAT_WATCH.
        private static void sendIntentWithRemoteInput(Context context, PendingIntent pendingIntent,
                RemoteInput[] remoteInputs, String resultKey, String reply)
                throws PendingIntent.CanceledException {
            Bundle results = new Bundle();
            results.putString(resultKey, reply);
            Intent fillInIntent = new Intent().addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
            RemoteInput.addResultsToIntent(remoteInputs, fillInIntent, results);

            // Send the pending intent filled in with the additional information from the new
            // intent.
            pendingIntent.send(context, 0 /* code */, fillInIntent);
        }
    }

    /**
     * Verifies that *not* setting a reply on the remote input of a notification action with type
     * 'text' and triggering the action's intent causes a null reply to be received in the
     * subsequent notificationclick event on the service worker.  Verifies that site engagement is
     * incremented appropriately.
     */
    @Test
    @TargetApi(Build.VERSION_CODES.KITKAT) // Notification.Action.actionIntent added in Android K.
    @CommandLineFlags.Add("enable-experimental-web-platform-features")
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testReplyToNotificationWithNoRemoteInput() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        // +0.5 engagement from navigating to the test page.
        Assert.assertEquals(0.5, getEngagementScoreBlocking(), 0);
        Notification notification = mNotificationTestRule.showAndGetNotification("MyNotification",
                "{ "
                        + " actions: [{action: 'myAction', title: 'reply', type: 'text'}],"
                        + " data: 'ACTION_REPLY'}");

        Assert.assertEquals("reply", notification.actions[0].title);
        notification.actions[0].actionIntent.send();

        // Check reply was received by the service worker (see android_test_worker.js).
        // Expect +1 engagement from interacting with the notification.
        waitForTitle("reply: null");
        Assert.assertEquals(1.5, getEngagementScoreBlocking(), 0);
    }

    /**
     * Verifies that the ONLY_ALERT_ONCE flag is not set when renotify is true.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationRenotifyProperty() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = mNotificationTestRule.showAndGetNotification(
                "MyNotification", "{ tag: 'myTag', renotify: true }");

        Assert.assertEquals(0, notification.flags & Notification.FLAG_ONLY_ALERT_ONCE);
    }

    /**
     * Verifies that notifications created with the "silent" flag do not inherit system defaults
     * in regards to their sound, vibration and light indicators.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationSilentProperty() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification =
                mNotificationTestRule.showAndGetNotification("MyNotification", "{ silent: true }");

        // Zero indicates that no defaults should be inherited from the system.
        Assert.assertEquals(0, notification.defaults);
    }

    private void verifyVibrationNotRequestedWhenDisabledInPrefs(String notificationOptions)
            throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        // Disable notification vibration in preferences.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setNotificationsVibrateEnabled(false);
            }
        });

        Notification notification =
                mNotificationTestRule.showAndGetNotification("MyNotification", notificationOptions);

        // Vibration should not be in the defaults.
        Assert.assertEquals(
                Notification.DEFAULT_ALL & ~Notification.DEFAULT_VIBRATE, notification.defaults);

        // There should be a custom no-op vibration pattern.
        Assert.assertEquals(1, notification.vibrate.length);
        Assert.assertEquals(0L, notification.vibrate[0]);
    }

    /**
     * Verifies that when notification vibration is disabled in preferences and no custom pattern is
     * specified, no vibration is requested from the framework.
     */
    @Test
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
    @Test
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
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationVibrateCustomPattern() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        // By default, vibration is enabled in notifications.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertTrue(PrefServiceBridge.getInstance().isNotificationsVibrateEnabled());
            }
        });

        Notification notification =
                mNotificationTestRule.showAndGetNotification("MyNotification", "{ vibrate: 42 }");

        // Vibration should not be in the defaults, a custom pattern was provided.
        Assert.assertEquals(
                Notification.DEFAULT_ALL & ~Notification.DEFAULT_VIBRATE, notification.defaults);

        // The custom pattern should have been passed along.
        Assert.assertEquals(2, notification.vibrate.length);
        Assert.assertEquals(0L, notification.vibrate[0]);
        Assert.assertEquals(42L, notification.vibrate[1]);
    }

    /**
     * Verifies that on Android M+, notifications which specify a badge will have that icon
     * fetched and included as the small icon in the notification and public version.
     * If the test target is L or below, verifies the small icon (and public small icon on L) is
     * the expected chrome logo.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testShowNotificationWithBadge() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification = mNotificationTestRule.showAndGetNotification(
                "MyNotification", "{badge: 'badge.png'}");

        Assert.assertEquals("MyNotification", NotificationTestUtil.getExtraTitle(notification));

        Context context = InstrumentationRegistry.getTargetContext();
        Bitmap smallIcon = NotificationTestUtil.getSmallIconFromNotification(context, notification);
        Assert.assertNotNull(smallIcon);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // Custom badges are only supported on M+.
            // 1. Check the notification badge.
            URL badgeUrl = new URL(mNotificationTestRule.getTestServer().getURL(
                    "/chrome/test/data/notifications/badge.png"));
            Bitmap bitmap = BitmapFactory.decodeStream(badgeUrl.openStream());
            Bitmap expected = bitmap.copy(bitmap.getConfig(), true);
            NotificationBuilderBase.applyWhiteOverlayToBitmap(expected);
            Assert.assertTrue(expected.sameAs(smallIcon));

            // 2. Check the public notification badge.
            Assert.assertNotNull(notification.publicVersion);
            Bitmap publicSmallIcon = NotificationTestUtil.getSmallIconFromNotification(
                    context, notification.publicVersion);
            Assert.assertNotNull(publicSmallIcon);
            Assert.assertEquals(expected.getWidth(), publicSmallIcon.getWidth());
            Assert.assertEquals(expected.getHeight(), publicSmallIcon.getHeight());
            Assert.assertTrue(expected.sameAs(publicSmallIcon));
        } else {
            Bitmap expected =
                    BitmapFactory.decodeResource(context.getResources(), R.drawable.ic_chrome);
            Assert.assertTrue(expected.sameAs(smallIcon));

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                // Public versions of notifications are only supported on L+.
                Assert.assertNotNull(notification.publicVersion);
                Bitmap publicSmallIcon = NotificationTestUtil.getSmallIconFromNotification(
                        context, notification.publicVersion);
                Assert.assertTrue(expected.sameAs(publicSmallIcon));
            }
        }
    }

    /**
     * Verifies that notifications which specify an icon will have that icon fetched, converted into
     * a Bitmap and included as the large icon in the notification.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    @RetryOnFailure
    public void testShowNotificationWithIcon() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification =
                mNotificationTestRule.showAndGetNotification("MyNotification", "{icon: 'red.png'}");

        Assert.assertEquals("MyNotification", NotificationTestUtil.getExtraTitle(notification));

        Context context = InstrumentationRegistry.getTargetContext();
        Bitmap largeIcon = NotificationTestUtil.getLargeIconFromNotification(context, notification);
        Assert.assertNotNull(largeIcon);
        Assert.assertEquals(Color.RED, largeIcon.getPixel(0, 0));
    }

    /**
     * Verifies that notifications which don't specify an icon will get an automatically generated
     * icon based on their origin. The size of these icons are dependent on the resolution of the
     * device the test is being ran on, so we create it again in order to compare.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    @RetryOnFailure
    public void testShowNotificationWithoutIcon() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Notification notification =
                mNotificationTestRule.showAndGetNotification("NoIconNotification", "{}");

        Assert.assertEquals("NoIconNotification", NotificationTestUtil.getExtraTitle(notification));

        Context context = InstrumentationRegistry.getTargetContext();
        Assert.assertNotNull(
                NotificationTestUtil.getLargeIconFromNotification(context, notification));

        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        Assert.assertNotNull(notificationBridge);

        // Create a second rounded icon for the test's origin, and compare its dimensions against
        // those of the icon associated to the notification itself.
        RoundedIconGenerator generator =
                NotificationBuilderBase.createIconGenerator(context.getResources());

        Bitmap generatedIcon = generator.generateIconForUrl(mNotificationTestRule.getOrigin());
        Assert.assertNotNull(generatedIcon);
        Assert.assertTrue(generatedIcon.sameAs(
                NotificationTestUtil.getLargeIconFromNotification(context, notification)));
    }

    /*
     * Verifies that starting the PendingIntent stored as the notification's content intent will
     * start up the associated Service Worker, where the JavaScript code will close the notification
     * by calling event.notification.close().
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @RetryOnFailure
    public void testNotificationContentIntentClosesNotification() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);
        // +0.5 engagement from navigating to the test page.
        Assert.assertEquals(0.5, getEngagementScoreBlocking(), 0);

        UserActionTester actionTester = new UserActionTester();

        Notification notification =
                mNotificationTestRule.showAndGetNotification("MyNotification", "{}");

        // Sending the PendingIntent resembles activating the notification.
        Assert.assertNotNull(notification.contentIntent);
        notification.contentIntent.send();

        // The Service Worker will close the notification upon receiving the notificationclick
        // event. This will eventually bubble up to a call to cancel() in the NotificationManager.
        // Expect +1 engagement from interacting with the notification.
        mNotificationTestRule.waitForNotificationManagerMutation();
        Assert.assertTrue(mNotificationTestRule.getNotificationEntries().isEmpty());
        Assert.assertEquals(1.5, getEngagementScoreBlocking(), 0);

        // This metric only applies on N+, where we schedule a job to handle the click.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            "Notifications.Android.JobStartDelay"));
        }

        // Clicking on a notification should record the right user metrics.
        assertThat(actionTester.toString(), getNotificationActions(actionTester),
                Matchers.contains(
                        "Notifications.Persistent.Shown", "Notifications.Persistent.Clicked"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Notifications.AppNotificationStatus"));
    }

    /**
     * Verifies that starting the PendingIntent stored as the notification's content intent will
     * start up the associated Service Worker, where the JavaScript code will create a new tab for
     * displaying the notification's event to the user.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @RetryOnFailure
    public void testNotificationContentIntentCreatesTab() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);

        Assert.assertEquals(
                "Expected the notification test page to be the sole tab in the current model", 1,
                mNotificationTestRule.getActivity().getCurrentTabModel().getCount());

        Notification notification = mNotificationTestRule.showAndGetNotification(
                "MyNotification", "{ data: 'ACTION_CREATE_TAB' }");

        // Sending the PendingIntent resembles activating the notification.
        Assert.assertNotNull(notification.contentIntent);
        notification.contentIntent.send();

        // The Service Worker, upon receiving the notificationclick event, will create a new tab
        // after which it closes the notification.
        mNotificationTestRule.waitForNotificationManagerMutation();
        Assert.assertTrue(mNotificationTestRule.getNotificationEntries().isEmpty());

        CriteriaHelper.pollInstrumentationThread(new Criteria("Expected a new tab to be created") {
            @Override
            public boolean isSatisfied() {
                return 2 == mNotificationTestRule.getActivity().getCurrentTabModel().getCount();
            }
        });
        // This metric only applies on N+, where we schedule a job to handle the click.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            "Notifications.Android.JobStartDelay"));
        }
    }

    /**
     * Verifies that creating a notification with an associated "tag" will cause any previous
     * notification with the same tag to be dismissed prior to being shown.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationTagReplacement() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);
        // +0.5 engagement from navigating to the test page.
        Assert.assertEquals(0.5, getEngagementScoreBlocking(), 0);

        mNotificationTestRule.runJavaScriptCodeInCurrentTab(
                "showNotification('MyNotification', {tag: 'myTag'});");
        mNotificationTestRule.waitForNotificationManagerMutation();
        List<NotificationEntry> notifications = mNotificationTestRule.getNotificationEntries();
        String tag = notifications.get(0).tag;
        int id = notifications.get(0).id;

        mNotificationTestRule.runJavaScriptCodeInCurrentTab(
                "showNotification('SecondNotification', {tag: 'myTag'});");
        mNotificationTestRule.waitForNotificationManagerMutation();

        // Verify that the notification was successfully replaced.
        notifications = mNotificationTestRule.getNotificationEntries();
        Assert.assertEquals(1, notifications.size());
        Assert.assertEquals("SecondNotification",
                NotificationTestUtil.getExtraTitle(notifications.get(0).notification));

        // Verify that for replaced notifications their tag was the same.
        Assert.assertEquals(tag, notifications.get(0).tag);

        // Verify that as always, the same integer is used, also for replaced notifications.
        Assert.assertEquals(id, notifications.get(0).id);
        Assert.assertEquals(NotificationPlatformBridge.PLATFORM_ID, notifications.get(0).id);

        // Engagement should not have changed since we didn't interact.
        Assert.assertEquals(0.5, getEngagementScoreBlocking(), 0);
    }

    /**
     * Verifies that multiple notifications without a tag can be opened and closed without
     * affecting eachother.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    public void testShowAndCloseMultipleNotifications() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);
        // +0.5 engagement from navigating to the test page.
        Assert.assertEquals(0.5, getEngagementScoreBlocking(), 0);

        // Open the first notification and verify it is displayed.
        mNotificationTestRule.runJavaScriptCodeInCurrentTab("showNotification('One');");
        mNotificationTestRule.waitForNotificationManagerMutation();
        List<NotificationEntry> notifications = mNotificationTestRule.getNotificationEntries();
        Assert.assertEquals(1, notifications.size());
        Notification notificationOne = notifications.get(0).notification;
        Assert.assertEquals("One", NotificationTestUtil.getExtraTitle(notificationOne));

        // Open the second notification and verify it is displayed.
        mNotificationTestRule.runJavaScriptCodeInCurrentTab("showNotification('Two');");
        mNotificationTestRule.waitForNotificationManagerMutation();
        notifications = mNotificationTestRule.getNotificationEntries();
        Assert.assertEquals(2, notifications.size());
        Notification notificationTwo = notifications.get(1).notification;
        Assert.assertEquals("Two", NotificationTestUtil.getExtraTitle(notificationTwo));

        // The same integer id is always used as it is not needed for uniqueness, we rely on the tag
        // for uniqueness when the replacement behavior is not needed.
        Assert.assertEquals(NotificationPlatformBridge.PLATFORM_ID, notifications.get(0).id);
        Assert.assertEquals(NotificationPlatformBridge.PLATFORM_ID, notifications.get(1).id);

        // As these notifications were not meant to replace eachother, they must not have the same
        // tag internally.
        Assert.assertFalse(notifications.get(0).tag.equals(notifications.get(1).tag));

        // Verify that the PendingIntent for content and delete is different for each notification.
        Assert.assertFalse(notificationOne.contentIntent.equals(notificationTwo.contentIntent));
        Assert.assertFalse(notificationOne.deleteIntent.equals(notificationTwo.deleteIntent));

        // Close the first notification and verify that only the second remains.
        // Sending the content intent resembles touching the notification. In response tho this the
        // notificationclick event is fired. The test service worker will close the notification
        // upon receiving the event.
        notificationOne.contentIntent.send();
        mNotificationTestRule.waitForNotificationManagerMutation();
        notifications = mNotificationTestRule.getNotificationEntries();
        Assert.assertEquals(1, notifications.size());
        Assert.assertEquals(
                "Two", NotificationTestUtil.getExtraTitle(notifications.get(0).notification));

        // Expect +1 engagement from interacting with the notification.
        Assert.assertEquals(1.5, getEngagementScoreBlocking(), 0);

        // Close the last notification and verify that none remain.
        notifications.get(0).notification.contentIntent.send();
        mNotificationTestRule.waitForNotificationManagerMutation();
        Assert.assertTrue(mNotificationTestRule.getNotificationEntries().isEmpty());

        // Expect +1 engagement from interacting with the notification.
        Assert.assertEquals(2.5, getEngagementScoreBlocking(), 0);
    }

    /**
     * Get Notification related actions, filter all other actions to avoid flakes.
     */
    private List<String> getNotificationActions(UserActionTester actionTester) {
        List<String> actions = new ArrayList<>(actionTester.getActions());
        Iterator<String> it = actions.iterator();
        while (it.hasNext()) {
            if (!it.next().startsWith("Notifications.")) {
                it.remove();
            }
        }
        return actions;
    }
}
