// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.push_messaging;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.annotation.SuppressLint;
import android.app.Notification;
import android.content.Context;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.util.Pair;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.notifications.NotificationTestRule;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.components.gcm_driver.GCMDriver;
import org.chromium.components.gcm_driver.GCMMessage;
import org.chromium.components.gcm_driver.instance_id.FakeInstanceIDWithSubtype;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for the Push API and the integration with the Notifications API on Android.
 */
// TODO(mvanouwerkerk): remove @SuppressLint once crbug.com/501900 is fixed.
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@SuppressLint("NewApi")
public class PushMessagingTest implements PushMessagingServiceObserver.Listener {
    @Rule
    public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public NotificationTestRule mNotificationTestRule = new NotificationTestRule();

    private static final String PUSH_TEST_PAGE =
            "/chrome/test/data/push_messaging/push_messaging_test_android.html";
    private static final String ABOUT_BLANK = "about:blank";
    private static final int TITLE_UPDATE_TIMEOUT_SECONDS = (int) scaleTimeout(5);
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chrome";

    private final CallbackHelper mMessageHandledHelper;
    private String mPushTestPage;

    public PushMessagingTest() {
        mMessageHandledHelper = new CallbackHelper();
    }

    @Before
    public void setUp() throws Exception {
        final PushMessagingServiceObserver.Listener listener = this;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FakeInstanceIDWithSubtype.clearDataAndSetEnabled(true);
                PushMessagingServiceObserver.setListenerForTesting(listener);
            }
        });
        mPushTestPage = mEmbeddedTestServerRule.getServer().getURL(PUSH_TEST_PAGE);
        mNotificationTestRule.loadUrl(mPushTestPage);
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PushMessagingServiceObserver.setListenerForTesting(null);
                FakeInstanceIDWithSubtype.clearDataAndSetEnabled(false);
            }
        });
    }

    @Override
    public void onMessageHandled() {
        mMessageHandledHelper.notifyCalled();
    }

    /**
     * Verifies that PushManager.subscribe() fails if Notifications permission was already denied.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "PushMessaging"})
    public void testNotificationsPermissionDenied() throws InterruptedException, TimeoutException {
        // Deny Notifications permission before trying to subscribe Push.
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.BLOCK, mEmbeddedTestServerRule.getOrigin());
        Assert.assertEquals("\"denied\"", runScriptBlocking("Notification.permission"));

        // Reload page to ensure the block is persisted.
        mNotificationTestRule.loadUrl(mPushTestPage);

        // PushManager.subscribePush() should fail immediately without showing an infobar.
        runScriptAndWaitForTitle("subscribePush()",
                "subscribe fail: NotAllowedError: Registration failed - permission denied");
        Assert.assertEquals(0, mNotificationTestRule.getInfoBars().size());

        // Notifications permission should still be denied.
        Assert.assertEquals("\"denied\"", runScriptBlocking("Notification.permission"));
    }

    /**
     * Verifies that PushManager.subscribe() fails if permission is dismissed or blocked.
     */
    //@MediumTest
    //@Feature({"Browser", "PushMessaging"})
    //@CommandLineFlags.Add("disable-features=ModalPermissionPrompts")
    @Test
    @DisabledTest
    public void testPushPermissionDenied() throws InterruptedException, TimeoutException {
        // Notifications permission should initially be prompt.
        Assert.assertEquals("\"default\"", runScriptBlocking("Notification.permission"));

        // PushManager.subscribePush() should show the notifications infobar.
        Assert.assertEquals(0, mNotificationTestRule.getInfoBars().size());
        runScript("subscribePush()");
        InfoBar infoBar = getInfobarBlocking();

        // Dismissing the infobar should cause subscribe() to fail.
        Assert.assertTrue(InfoBarUtil.clickCloseButton(infoBar));
        waitForInfobarToClose();
        waitForTitle(mNotificationTestRule.getActivity().getActivityTab(),
                "subscribe fail: NotAllowedError: Registration failed - permission denied");

        // Notifications permission should still be prompt.
        Assert.assertEquals("\"default\"", runScriptBlocking("Notification.permission"));

        runScriptAndWaitForTitle("sendToTest('reset title')",
                "clearCachedVerificationsForTesting title");

        // PushManager.subscribePush() should show the notifications infobar again.
        runScript("subscribePush()");
        infoBar = getInfobarBlocking();

        // Denying the infobar should cause subscribe() to fail.
        Assert.assertTrue(InfoBarUtil.clickSecondaryButton(infoBar));
        waitForInfobarToClose();
        waitForTitle(mNotificationTestRule.getActivity().getActivityTab(),
                "subscribe fail: NotAllowedError: Registration failed - permission denied");

        // This should have caused notifications permission to become denied.
        Assert.assertEquals("\"denied\"", runScriptBlocking("Notification.permission"));

        // Reload page to ensure the block is persisted.
        mNotificationTestRule.loadUrl(mPushTestPage);

        // PushManager.subscribePush() should now fail immediately without showing an infobar.
        runScriptAndWaitForTitle("subscribePush()",
                "subscribe fail: NotAllowedError: Registration failed - permission denied");
        Assert.assertEquals(0, mNotificationTestRule.getInfoBars().size());

        // Notifications permission should still be denied.
        Assert.assertEquals("\"denied\"", runScriptBlocking("Notification.permission"));
    }

    /**
     * Verifies that PushManager.subscribe() requests permission correctly.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "PushMessaging"})
    @CommandLineFlags.Add("disable-features=ModalPermissionPrompts")
    public void testPushPermissionGranted() throws InterruptedException, TimeoutException {
        // Notifications permission should initially be prompt.
        Assert.assertEquals("\"default\"", runScriptBlocking("Notification.permission"));

        // PushManager.subscribePush() should show the notifications infobar.
        Assert.assertEquals(0, mNotificationTestRule.getInfoBars().size());
        runScript("subscribePush()");
        InfoBar infoBar = getInfobarBlocking();

        // Accepting the infobar should cause subscribe() to succeed.
        Assert.assertTrue(InfoBarUtil.clickPrimaryButton(infoBar));
        waitForInfobarToClose();
        waitForTitle(mNotificationTestRule.getActivity().getActivityTab(), "subscribe ok");

        // This should have caused notifications permission to become granted.
        Assert.assertEquals("\"granted\"", runScriptBlocking("Notification.permission"));
    }

    /**
     * Verifies that a notification can be shown from a push event handler in the service worker.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "PushMessaging"})
    @RetryOnFailure
    public void testPushAndShowNotification() throws InterruptedException, TimeoutException {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mEmbeddedTestServerRule.getOrigin());
        runScriptAndWaitForTitle("subscribePush()", "subscribe ok");

        Pair<String, String> appIdAndSenderId =
                FakeInstanceIDWithSubtype.getSubtypeAndAuthorizedEntityOfOnlyToken();
        sendPushAndWaitForCallback(appIdAndSenderId);
        NotificationEntry notificationEntry = mNotificationTestRule.waitForNotification();
        Assert.assertEquals("push notification 1",
                notificationEntry.notification.extras.getString(Notification.EXTRA_TITLE));
    }

    /**
     * Verifies that the default notification is shown when no notification is shown from the push
     * event handler while no tab is visible for the origin, and grace has been exceeded.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "PushMessaging"})
    @RetryOnFailure
    public void testDefaultNotification() throws InterruptedException, TimeoutException {
        // Start off using the tab loaded in setUp().
        Assert.assertEquals(1, mNotificationTestRule.getActivity().getCurrentTabModel().getCount());
        Tab tab = mNotificationTestRule.getActivity().getActivityTab();
        Assert.assertEquals(mPushTestPage, tab.getUrl());
        Assert.assertFalse(tab.isHidden());

        // Set up the push subscription and capture its details.
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mEmbeddedTestServerRule.getOrigin());
        runScriptAndWaitForTitle("subscribePush()", "subscribe ok");
        Pair<String, String> appIdAndSenderId =
                FakeInstanceIDWithSubtype.getSubtypeAndAuthorizedEntityOfOnlyToken();

        // Make the tab invisible by opening another one with a different origin.
        mNotificationTestRule.loadUrlInNewTab(ABOUT_BLANK);
        Assert.assertEquals(2, mNotificationTestRule.getActivity().getCurrentTabModel().getCount());
        Assert.assertEquals(
                ABOUT_BLANK, mNotificationTestRule.getActivity().getActivityTab().getUrl());
        Assert.assertTrue(tab.isHidden());

        // The first time a push event is fired and no notification is shown from the service
        // worker, grace permits it so no default notification is shown.
        runScriptAndWaitForTitle("setNotifyOnPush(false)", "setNotifyOnPush false ok", tab);
        sendPushAndWaitForCallback(appIdAndSenderId);

        // After grace runs out a default notification will be shown.
        sendPushAndWaitForCallback(appIdAndSenderId);
        NotificationEntry notificationEntry = mNotificationTestRule.waitForNotification();
        Assert.assertThat(
                notificationEntry.tag, Matchers.containsString("user_visible_auto_notification"));

        // When another push does show a notification, the default notification is automatically
        // dismissed (an additional mutation) so there is only one left in the end.
        runScriptAndWaitForTitle("setNotifyOnPush(true)", "setNotifyOnPush true ok", tab);
        sendPushAndWaitForCallback(appIdAndSenderId);
        mNotificationTestRule.waitForNotificationManagerMutation();
        notificationEntry = mNotificationTestRule.waitForNotification();
        Assert.assertEquals("push notification 1",
                notificationEntry.notification.extras.getString(Notification.EXTRA_TITLE));
    }

    /**
     * Runs {@code script} in the current tab but does not wait for the result.
     */
    private void runScript(String script) {
        JavaScriptUtils.executeJavaScript(mNotificationTestRule.getWebContents(), script);
    }

    /**
     * Runs {@code script} in the current tab and returns its synchronous result in JSON format.
     */
    private String runScriptBlocking(String script) throws InterruptedException, TimeoutException {
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mNotificationTestRule.getWebContents(), script);
    }

    /**
     * Runs {@code script} in the current tab and waits for the tab title to change to
     * {@code expectedTitle}.
     */
    private void runScriptAndWaitForTitle(String script, String expectedTitle)
            throws InterruptedException {
        runScriptAndWaitForTitle(
                script, expectedTitle, mNotificationTestRule.getActivity().getActivityTab());
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

    private void sendPushAndWaitForCallback(Pair<String, String> appIdAndSenderId)
            throws InterruptedException, TimeoutException {
        final String appId = appIdAndSenderId.first;
        final String senderId = appIdAndSenderId.second;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Context context = InstrumentationRegistry.getInstrumentation()
                                          .getTargetContext()
                                          .getApplicationContext();

                Bundle extras = new Bundle();
                extras.putString("subtype", appId);

                GCMMessage message = new GCMMessage(senderId, extras);
                try {
                    ChromeBrowserInitializer.getInstance(context).handleSynchronousStartup();
                    GCMDriver.dispatchMessage(message);
                } catch (ProcessInitException e) {
                    Assert.fail("Chrome browser failed to initialize.");
                }
            }
        });
        mMessageHandledHelper.waitForCallback(mMessageHandledHelper.getCallCount());
    }

    @SuppressWarnings("MissingFail")
    private void waitForTitle(Tab tab, String expectedTitle) throws InterruptedException {
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
}
