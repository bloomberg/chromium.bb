// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.push_messaging;

import android.annotation.SuppressLint;
import android.app.Notification;
import android.content.Context;
import android.os.Bundle;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.notifications.NotificationTestBase;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.components.gcm_driver.FakeGoogleCloudMessagingSubscriber;
import org.chromium.components.gcm_driver.GCMDriver;

/**
 * Instrumentation tests for the Push API and the integration with the Notifications API on Android.
 */
// TODO(mvanouwerkerk): remove @SuppressLint once crbug.com/501900 is fixed.
@SuppressLint("NewApi")
public class PushMessagingTest extends NotificationTestBase {
    private static final String PUSH_TEST_PAGE = TestHttpServerClient.getUrl(
            "chrome/test/data/push_messaging/push_messaging_test_android.html");
    private static final String SENDER_ID_BUNDLE_KEY = "from";
    private static final int TITLE_UPDATE_TIMEOUT_SECONDS = 5;

    /**
     * Verifies that a notification can be shown from a push event handler in the service worker.
     */
    @MediumTest
    @Feature({"Browser", "PushMessaging"})
    public void testPushAndShowNotification() throws Exception {
        FakeGoogleCloudMessagingSubscriber subscriber = new FakeGoogleCloudMessagingSubscriber();
        GCMDriver.overrideSubscriberForTesting(subscriber);

        loadUrl(PUSH_TEST_PAGE);
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);
        runScriptAndWaitForTitle("subscribePush()", "subscribe ok");

        sendPushMessage(subscriber.getLastSubscribeSubtype(), subscriber.getLastSubscribeSource());
        NotificationEntry notificationEntry = waitForNotification();
        assertEquals("push notification",
                notificationEntry.notification.extras.getString(Notification.EXTRA_TITLE));
    }

    /**
     * Runs {@code script} in the current tab and waits for the tab title to change to
     * {@code expectedResult}.
     */
    private void runScriptAndWaitForTitle(String script, String expectedResult) throws Exception {
        runJavaScriptCodeInCurrentTab(script);
        TabTitleObserver titleObserver =
                new TabTitleObserver(getActivity().getActivityTab(), expectedResult);
        titleObserver.waitForTitleUpdate(TITLE_UPDATE_TIMEOUT_SECONDS);
    }

    private void sendPushMessage(final String appId, final String senderId) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Context context = getInstrumentation().getTargetContext().getApplicationContext();
                Bundle extras = new Bundle();
                extras.putString(SENDER_ID_BUNDLE_KEY, senderId);
                GCMDriver.onMessageReceived(context, appId, extras);
            }
        });
    }
}
