// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.push_messaging;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.annotation.SuppressLint;
import android.app.Notification;
import android.content.Context;
import android.os.Bundle;
import android.test.MoreAsserts;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.notifications.NotificationTestBase;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.components.gcm_driver.FakeGoogleCloudMessagingSubscriber;
import org.chromium.components.gcm_driver.GCMDriver;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for the Push API and the integration with the Notifications API on Android.
 */
// TODO(mvanouwerkerk): remove @SuppressLint once crbug.com/501900 is fixed.
@SuppressLint("NewApi")
public class PushMessagingTest
        extends NotificationTestBase implements PushMessagingServiceObserver.Listener {
    private static final String PUSH_TEST_PAGE =
            "/chrome/test/data/push_messaging/push_messaging_test_android.html";
    private static final String ABOUT_BLANK = "about:blank";
    private static final String SENDER_ID_BUNDLE_KEY = "from";
    private static final int TITLE_UPDATE_TIMEOUT_SECONDS = (int) scaleTimeout(5);

    private final CallbackHelper mMessageHandledHelper;
    private String mPushTestPage;

    public PushMessagingTest() {
        mMessageHandledHelper = new CallbackHelper();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        final PushMessagingServiceObserver.Listener listener = this;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PushMessagingServiceObserver.setListenerForTesting(listener);
            }
        });
        mPushTestPage = getTestServer().getURL(PUSH_TEST_PAGE);
    }

    @Override
    protected void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PushMessagingServiceObserver.setListenerForTesting(null);
            }
        });
        super.tearDown();
    }

    @Override
    public void onMessageHandled() {
        mMessageHandledHelper.notifyCalled();
    }

    /**
     * Verifies that a notification can be shown from a push event handler in the service worker.
     */
    @MediumTest
    @Feature({"Browser", "PushMessaging"})
    public void testPushAndShowNotification() throws InterruptedException, TimeoutException {
        FakeGoogleCloudMessagingSubscriber subscriber = new FakeGoogleCloudMessagingSubscriber();
        GCMDriver.overrideSubscriberForTesting(subscriber);

        loadUrl(mPushTestPage);
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);
        runScriptAndWaitForTitle("subscribePush()", "subscribe ok");

        sendPushAndWaitForCallback(
                subscriber.getLastSubscribeSubtype(), subscriber.getLastSubscribeSource());
        NotificationEntry notificationEntry = waitForNotification();
        assertEquals("push notification 1",
                notificationEntry.notification.extras.getString(Notification.EXTRA_TITLE));
    }

    /**
     * Verifies that the default notification is shown when no notification is shown from the push
     * event handler while no tab is visible for the origin, and grace has been exceeded.
     */
    @LargeTest
    @Feature({"Browser", "PushMessaging"})
    public void testDefaultNotification() throws InterruptedException, TimeoutException {
        FakeGoogleCloudMessagingSubscriber subscriber = new FakeGoogleCloudMessagingSubscriber();
        GCMDriver.overrideSubscriberForTesting(subscriber);

        // Load the push test page into the first tab.
        loadUrl(mPushTestPage);
        assertEquals(1, getActivity().getCurrentTabModel().getCount());
        Tab tab = getActivity().getActivityTab();
        assertEquals(mPushTestPage, tab.getUrl());
        assertFalse(tab.isHidden());

        // Set up the push subscription and capture its details.
        setNotificationContentSettingForCurrentOrigin(ContentSetting.ALLOW);
        runScriptAndWaitForTitle("subscribePush()", "subscribe ok");
        String appId = subscriber.getLastSubscribeSubtype();
        String senderId = subscriber.getLastSubscribeSource();

        // Make the tab invisible by opening another one with a different origin.
        loadUrlInNewTab(ABOUT_BLANK);
        assertEquals(2, getActivity().getCurrentTabModel().getCount());
        assertEquals(ABOUT_BLANK, getActivity().getActivityTab().getUrl());
        assertTrue(tab.isHidden());

        // The first time a push event is fired and no notification is shown from the service
        // worker, grace permits it so no default notification is shown.
        runScriptAndWaitForTitle("setNotifyOnPush(false)", "setNotifyOnPush false ok", tab);
        sendPushAndWaitForCallback(appId, senderId);

        // After grace runs out a default notification will be shown.
        sendPushAndWaitForCallback(appId, senderId);
        NotificationEntry notificationEntry = waitForNotification();
        MoreAsserts.assertContainsRegex("user_visible_auto_notification", notificationEntry.tag);

        // When another push does show a notification, the default notification is automatically
        // dismissed (an additional mutation) so there is only one left in the end.
        runScriptAndWaitForTitle("setNotifyOnPush(true)", "setNotifyOnPush true ok", tab);
        sendPushAndWaitForCallback(appId, senderId);
        waitForNotificationManagerMutation();
        notificationEntry = waitForNotification();
        assertEquals("push notification 1",
                notificationEntry.notification.extras.getString(Notification.EXTRA_TITLE));
    }

    /**
     * Runs {@code script} in the current tab and waits for the tab title to change to
     * {@code expectedTitle}.
     */
    private void runScriptAndWaitForTitle(String script, String expectedTitle)
            throws InterruptedException {
        runScriptAndWaitForTitle(script, expectedTitle, getActivity().getActivityTab());
    }

    /**
     * Runs {@code script} in {@code tab} and waits for the tab title to change to
     * {@code expectedTitle}.
     */
    private void runScriptAndWaitForTitle(String script, String expectedTitle, Tab tab)
            throws InterruptedException {
        JavaScriptUtils.executeJavaScript(tab.getWebContents(), script);
        waitForTitle(tab, expectedTitle);
    }

    private void sendPushAndWaitForCallback(final String appId, final String senderId)
            throws InterruptedException, TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Context context = getInstrumentation().getTargetContext().getApplicationContext();
                Bundle extras = new Bundle();
                extras.putString(SENDER_ID_BUNDLE_KEY, senderId);
                GCMDriver.onMessageReceived(context, appId, extras);
            }
        });
        mMessageHandledHelper.waitForCallback(mMessageHandledHelper.getCallCount());
    }

    private void waitForTitle(Tab tab, String expectedTitle) throws InterruptedException {
        TabTitleObserver titleObserver = new TabTitleObserver(tab, expectedTitle);
        try {
            titleObserver.waitForTitleUpdate(TITLE_UPDATE_TIMEOUT_SECONDS);
        } catch (TimeoutException e) {
            // The title is not as expected, this assertion neatly logs what the difference is.
            assertEquals(expectedTitle, tab.getTitle());
        }
    }
}
