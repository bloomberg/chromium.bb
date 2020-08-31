// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.notifications.NotificationIntentInterceptor.INTENT_ACTION;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowNotificationManager;
import org.robolectric.shadows.ShadowPendingIntent;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.ChromeNotification;
import org.chromium.components.browser_ui.notifications.ChromeNotificationBuilder;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Test to verify {@link NotificationIntentInterceptor} can intercept the {@link PendingIntent} and
 * track metrics correctly.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(shadows = {ShadowNotificationManager.class, ShadowPendingIntent.class,
                ShadowRecordHistogram.class})

public class NotificationIntentInterceptorTest {
    private static final String TEST_NOTIFICATION_TITLE = "Test notification title";
    private static final String TEST_NOTIFICATION_ACTION_TITLE = "Test notification action title";
    private static final String EXTRA_INTENT_TYPE =
            "NotificationIntentInterceptorTest.EXTRA_INTENT_TYPE";

    private Context mContext;
    private ShadowNotificationManager mShadowNotificationManager;
    private TestReceiver mReceiver;

    /**
     * When the user clicks the notification, the intent will go through {@link
     * NotificationIntentInterceptor.Receiver} to track metrics and then arrive at this {@link
     * BroadcastReceiver}.
     */
    public static final class TestReceiver extends BroadcastReceiver {
        private static final String TEST_ACTION =
                "NotificationIntentInterceptorTest.TestReceiver.TEST_ACTION";
        private Intent mIntentReceived;

        public Intent intentReceived() {
            return mIntentReceived;
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            mIntentReceived = intent;
        }
    }

    @Before
    public void setUp() throws Exception {
        ShadowLog.stream = System.out;
        ShadowRecordHistogram.reset();
        mContext = ApplicationProvider.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mContext);
        mShadowNotificationManager = shadowOf(
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE));
        mContext.registerReceiver(
                new NotificationIntentInterceptor.Receiver(), new IntentFilter(INTENT_ACTION));
        mReceiver = new TestReceiver();
        mContext.registerReceiver(mReceiver, new IntentFilter(TestReceiver.TEST_ACTION));
    }

    @After
    public void tearDown() {
        ShadowRecordHistogram.reset();
    }

    // Builds a simple notification used in tests.
    private ChromeNotification buildSimpleNotification(String title) {
        NotificationMetadata metaData = new NotificationMetadata(
                NotificationUmaTracker.SystemNotificationType.DOWNLOAD_FILES, null, 0);
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory.createChromeNotificationBuilder(true /* preferCompat */,
                        ChromeChannelDefinitions.ChannelId.DOWNLOADS,
                        null /* remoteAppPackageName */, metaData);

        // Set content intent.
        Intent contentIntent = new Intent(TestReceiver.TEST_ACTION);
        contentIntent.putExtra(
                EXTRA_INTENT_TYPE, NotificationIntentInterceptor.IntentType.CONTENT_INTENT);
        PendingIntentProvider contentPendingIntent =
                PendingIntentProvider.getBroadcast(mContext, 0, contentIntent, 0);
        builder.setContentIntent(contentPendingIntent);
        builder.setContentTitle(title);
        builder.setSmallIcon(R.drawable.offline_pin);

        // Add a button.
        Intent actionIntent = new Intent(TestReceiver.TEST_ACTION);
        actionIntent.putExtra(
                EXTRA_INTENT_TYPE, NotificationIntentInterceptor.IntentType.ACTION_INTENT);

        // Need to use a different request code here since content intent and action intent shares
        // the same broadcast receiver.
        PendingIntentProvider actionPendingIntent =
                PendingIntentProvider.getBroadcast(mContext, /*requestCode=*/1, actionIntent, 0);
        builder.addAction(0, TEST_NOTIFICATION_ACTION_TITLE, actionPendingIntent,
                NotificationUmaTracker.ActionType.DOWNLOAD_PAUSE);

        return builder.buildChromeNotification();
    }

    /**
     * Verifies {@link Notification#contentIntent} can be intercepted by {@link
     * NotificationIntentInterceptor}.
     */
    @Test
    public void testContentIntentInterception() throws Exception {
        // Send notification.
        new NotificationManagerProxyImpl(mContext).notify(
                buildSimpleNotification(TEST_NOTIFICATION_TITLE));

        // Simulates a notification click.
        Notification notification = mShadowNotificationManager.getAllNotifications().get(0);
        Assert.assertEquals(TEST_NOTIFICATION_TITLE,
                notification.extras.getCharSequence(Notification.EXTRA_TITLE).toString());
        notification.contentIntent.send();
        Robolectric.flushForegroundThreadScheduler();

        // Verify the intent and histograms recorded.
        Intent receivedIntent = mReceiver.intentReceived();
        Assert.assertEquals(receivedIntent.getExtras().getInt(EXTRA_INTENT_TYPE),
                NotificationIntentInterceptor.IntentType.CONTENT_INTENT);
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Content.Click",
                        NotificationUmaTracker.SystemNotificationType.DOWNLOAD_FILES));
    }

    /**
     * Verifies {@link Notification#deleteIntent} can be intercepted by {@link
     * NotificationIntentInterceptor}.
     */
    @Test
    public void testDeleteIntentInterception() throws Exception {
        // Send notification.
        new NotificationManagerProxyImpl(mContext).notify(
                buildSimpleNotification(TEST_NOTIFICATION_TITLE));

        // Simulates a notification cancel.
        Notification notification = mShadowNotificationManager.getAllNotifications().get(0);
        Assert.assertEquals(TEST_NOTIFICATION_TITLE,
                notification.extras.getCharSequence(Notification.EXTRA_TITLE).toString());
        notification.deleteIntent.send();
        Robolectric.flushForegroundThreadScheduler();

        // Verify the histogram.
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Dismiss",
                        NotificationUmaTracker.SystemNotificationType.DOWNLOAD_FILES));
        Assert.assertNull(mReceiver.intentReceived());
    }

    /**
     * Verifies button clicks can be intercepted by {@link NotificationIntentInterceptor}.
     */
    @Test
    public void testActionIntentInterception() throws Exception {
        // Send notification.
        new NotificationManagerProxyImpl(mContext).notify(
                buildSimpleNotification(TEST_NOTIFICATION_TITLE));

        // Simulates a button click.
        Notification notification = mShadowNotificationManager.getAllNotifications().get(0);
        Assert.assertEquals(TEST_NOTIFICATION_TITLE,
                notification.extras.getCharSequence(Notification.EXTRA_TITLE).toString());
        Assert.assertNotNull(notification.actions);
        Assert.assertEquals(1, notification.actions.length);
        Notification.Action action = notification.actions[0];
        Assert.assertNotNull(action.actionIntent);
        action.actionIntent.send();

        Robolectric.flushForegroundThreadScheduler();

        // Verify the intent and histograms recorded.
        Intent receivedIntent = mReceiver.intentReceived();
        Assert.assertEquals(NotificationIntentInterceptor.IntentType.ACTION_INTENT,
                receivedIntent.getExtras().getInt(EXTRA_INTENT_TYPE));
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Action.Click",
                        NotificationUmaTracker.ActionType.DOWNLOAD_PAUSE));
    }
}
