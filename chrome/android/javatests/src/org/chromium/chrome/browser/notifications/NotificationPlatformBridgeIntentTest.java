// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.website.SingleCategoryPreferences;
import org.chromium.chrome.browser.preferences.website.SingleWebsitePreferences;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Instrumentation tests for the Notification Platform Bridge.
 *
 * Exercises the handling of intents and explicitly does not do anything in startMainActivity so
 * that the responsibility for correct initialization, e.g. loading the native library, lies with
 * the code exercised by this test.
 */
public class NotificationPlatformBridgeIntentTest
        extends ChromeActivityTestCaseBase<ChromeActivity> {
    /**
     * Name of the Intent extra holding the notification id. This is set by the framework when a
     * notification preferences intent has been triggered from there, which could be one of the
     * setting gears in system UI.
     */
    public static final String EXTRA_NOTIFICATION_ID = "notification_id";

    public NotificationPlatformBridgeIntentTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        // Don't do anything here. The responsibility for correct initialization, e.g. loading the
        // native library, lies with the code exercised by this test.
    }

    /**
     * Tests the scenario where the user clicks "App settings" in the Android App notifications
     * screen. In this scenario the intent does not have the tag extra which holds the origin. This
     * means that the preferences cannot be shown for a specific site, and Chrome falls back to
     * showing the notification preferences for all sites.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testLaunchNotificationPreferencesForCategory() throws Exception {
        assertFalse("The native library should not be loaded yet", LibraryLoader.isInitialized());

        final Context context = getInstrumentation().getTargetContext().getApplicationContext();

        final Intent intent =
                new Intent(Intent.ACTION_MAIN)
                        .addCategory(Notification.INTENT_CATEGORY_NOTIFICATION_PREFERENCES)
                        .setClassName(context, ChromeLauncherActivity.class.getName())
                        .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Preferences activity = ActivityUtils.waitForActivity(
                getInstrumentation(), Preferences.class, new Runnable() {
                    @Override
                    public void run() {
                        context.startActivity(intent);
                    }
                });
        assertNotNull("Could not find the Preferences activity", activity);

        SingleCategoryPreferences fragment =
                ActivityUtils.waitForFragmentToAttach(activity, SingleCategoryPreferences.class);
        assertNotNull("Could not find the SingleCategoryPreferences fragment", fragment);
    }

    /**
     * Tests the scenario where the user clicks the settings gear on a notification that was flipped
     * through a long press. In this scenario the intent has the tag extra which holds the origin
     * that created the notification. Chrome will show the preferences for this specific site.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testLaunchNotificationPreferencesForWebsite() throws Exception {
        assertFalse("The native library should not be loaded yet", LibraryLoader.isInitialized());

        final Context context = getInstrumentation().getTargetContext().getApplicationContext();

        final Intent intent =
                new Intent(Intent.ACTION_MAIN)
                        .addCategory(Notification.INTENT_CATEGORY_NOTIFICATION_PREFERENCES)
                        .setClassName(context, ChromeLauncherActivity.class.getName())
                        .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                        .putExtra(EXTRA_NOTIFICATION_ID, NotificationPlatformBridge.PLATFORM_ID)
                        .putExtra(NotificationConstants.EXTRA_NOTIFICATION_TAG,
                                NotificationPlatformBridge.makePlatformTag(
                                        42L /* persistentNotificationId */, "https://example.com",
                                        null /* tag */));

        Preferences activity = ActivityUtils.waitForActivity(
                getInstrumentation(), Preferences.class, new Runnable() {
                    @Override
                    public void run() {
                        context.startActivity(intent);
                    }
                });
        assertNotNull("Could not find the Preferences activity", activity);

        SingleWebsitePreferences fragment =
                ActivityUtils.waitForFragmentToAttach(activity, SingleWebsitePreferences.class);
        assertNotNull("Could not find the SingleWebsitePreferences fragment", fragment);
    }

    /**
     * Tests the browser initialization code when a notification has been activated. This will be
     * routed through the NotificationService which starts the browser process, which in turn will
     * create an instance of the NotificationPlatformBridge.
     *
     * The created intent does not carry significant data and is expected to fail, but has to be
     * sufficient for the Java code to trigger start-up of the browser process.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testLaunchProcessForNotificationActivation() throws Exception {
        assertFalse("The native library should not be loaded yet", LibraryLoader.isInitialized());
        assertNull(NotificationPlatformBridge.getInstanceForTests());

        Context context = getInstrumentation().getTargetContext().getApplicationContext();

        Intent intent = new Intent(NotificationConstants.ACTION_CLICK_NOTIFICATION);
        intent.setClass(context, NotificationService.Receiver.class);

        long persistentId = 42;

        intent.putExtra(NotificationConstants.EXTRA_PERSISTENT_NOTIFICATION_ID, persistentId);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_ID, "Default");
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_ORIGIN, "example.com");
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_TAG, "tag");

        PendingIntent pendingIntent = PendingIntent.getBroadcast(
                context, 0 /* request code */, intent, PendingIntent.FLAG_UPDATE_CURRENT);

        // Send the pending intent. This will begin starting up the browser process.
        pendingIntent.send();

        CriteriaHelper.pollUiThread(new Criteria("Browser process was never started.") {
            @Override
            public boolean isSatisfied() {
                return NotificationPlatformBridge.getInstanceForTests() != null;
            }
        });

        assertTrue("The native library should be loaded now", LibraryLoader.isInitialized());
        assertNotNull(NotificationPlatformBridge.getInstanceForTests());
    }
}
