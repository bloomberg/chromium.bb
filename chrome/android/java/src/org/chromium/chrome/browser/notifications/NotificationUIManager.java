// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.support.v4.app.NotificationCompat;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.website.SingleWebsitePreferences;
import org.chromium.chrome.browser.preferences.website.WebsitePreferences;
import org.chromium.chrome.browser.preferences.website.WebsiteSettingsCategoryFilter;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;

/**
 * Provides the ability for the NotificationUIManagerAndroid to talk to the Android platform
 * notification manager.
 *
 * This class should only be used on the UI thread.
 */
public class NotificationUIManager {
    private static final String TAG = NotificationUIManager.class.getSimpleName();

    private static final int NOTIFICATION_ICON_BG_COLOR = Color.rgb(150, 150, 150);
    private static final int NOTIFICATION_TEXT_SIZE_DP = 28;

    private static NotificationUIManager sInstance;
    private static NotificationManagerProxy sNotificationManagerOverride;

    private final long mNativeNotificationManager;

    private final Context mAppContext;
    private final NotificationManagerProxy mNotificationManager;

    private RoundedIconGenerator mIconGenerator;

    private int mLastNotificationId;

    /**
     * Creates a new instance of the NotificationUIManager.
     *
     * @param nativeNotificationManager Instance of the NotificationUIManagerAndroid class.
     * @param context Application context for this instance of Chrome.
     */
    @CalledByNative
    private static NotificationUIManager create(long nativeNotificationManager, Context context) {
        if (sInstance != null) {
            throw new IllegalStateException("There must only be a single NotificationUIManager.");
        }

        sInstance = new NotificationUIManager(nativeNotificationManager, context);
        return sInstance;
    }

    /**
     * Overrides the notification manager which is to be used for displaying Notifications on the
     * Android framework. Should only be used for testing. Tests are expected to clean up after
     * themselves by setting this to NULL again.
     *
     * @param proxy The notification manager instance to use instead of the system's.
     */
    @VisibleForTesting
    public static void overrideNotificationManagerForTesting(
            NotificationManagerProxy notificationManager) {
        sNotificationManagerOverride = notificationManager;
    }

    private NotificationUIManager(long nativeNotificationManager, Context context) {
        mNativeNotificationManager = nativeNotificationManager;
        mAppContext = context.getApplicationContext();

        if (sNotificationManagerOverride != null) {
            mNotificationManager = sNotificationManagerOverride;
        } else {
            mNotificationManager = new NotificationManagerProxyImpl(
                    (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE));
        }

        mLastNotificationId = 0;
    }

    /**
     * Marks the current instance as being freed, allowing for a new NotificationUIManager
     * object to be initialized.
     */
    @CalledByNative
    private void destroy() {
        assert sInstance == this;
        sInstance = null;
    }

    /**
     * Invoked by the NotificationService when a Notification intent has been received. There may
     * not be an active instance of the NotificationUIManager at this time, so inform the native
     * side through a static method, initialzing the manager if needed.
     *
     * @param intent The intent as received by the Notification service.
     * @return Whether the event could be handled by the native Notification manager.
     */
    public static boolean dispatchNotificationEvent(Intent intent) {
        if (sInstance == null) {
            nativeInitializeNotificationUIManager();
            if (sInstance == null) {
                Log.e(TAG, "Unable to initialize the native NotificationUIManager.");
                return false;
            }
        }

        String notificationId = intent.getStringExtra(NotificationConstants.EXTRA_NOTIFICATION_ID);
        if (NotificationConstants.ACTION_CLICK_NOTIFICATION.equals(intent.getAction())) {
            if (!intent.hasExtra(NotificationConstants.EXTRA_NOTIFICATION_DATA)
                    || !intent.hasExtra(NotificationConstants.EXTRA_NOTIFICATION_PLATFORM_ID)) {
                Log.e(TAG, "Not all required notification data has been set in the intent.");
                return false;
            }

            int platformId =
                    intent.getIntExtra(NotificationConstants.EXTRA_NOTIFICATION_PLATFORM_ID, -1);
            byte[] notificationData =
                    intent.getByteArrayExtra(NotificationConstants.EXTRA_NOTIFICATION_DATA);
            return sInstance.onNotificationClicked(notificationId, platformId, notificationData);
        }

        if (NotificationConstants.ACTION_CLOSE_NOTIFICATION.equals(intent.getAction())) {
            return sInstance.onNotificationClosed(notificationId);
        }

        Log.e(TAG, "Unrecognized Notification action: " + intent.getAction());
        return false;
    }

    /**
     * Launches the notifications preferences screen. If the received intent indicates it came
     * from the gear button on a flipped notification, this launches the site specific preferences
     * screen.
     *
     * @param context The context that received the intent.
     * @param incomingIntent The received intent.
     */
    public static void launchNotificationPreferences(Context context, Intent incomingIntent) {
        // Use the application context because it lives longer. When using he given context, it
        // may be stopped before the preferences intent is handled.
        Context applicationContext = context.getApplicationContext();

        // If the user touched the settings cog on a flipped notification there will be a
        // notification tag extra. From the tag we can read the origin of the notification, and use
        // that to open the settings screen for that specific origin.
        String notificationTag =
                incomingIntent.getStringExtra(NotificationConstants.EXTRA_NOTIFICATION_TAG);
        boolean launchSingleWebsitePreferences = notificationTag != null;

        String fragmentName = launchSingleWebsitePreferences
                ? SingleWebsitePreferences.class.getName()
                : WebsitePreferences.class.getName();
        Intent preferencesIntent =
                PreferencesLauncher.createIntentForSettingsPage(applicationContext, fragmentName);

        Bundle fragmentArguments;
        if (launchSingleWebsitePreferences) {
            // All preferences for a specific origin.
            String[] tagParts =
                    notificationTag.split(NotificationConstants.NOTIFICATION_TAG_SEPARATOR);
            assert tagParts.length >= 2;
            String origin = tagParts[0];
            fragmentArguments = SingleWebsitePreferences.createFragmentArgsForSite(origin);
        } else {
            // Notification preferences for all origins.
            fragmentArguments = new Bundle();
            fragmentArguments.putString(WebsitePreferences.EXTRA_CATEGORY,
                    WebsiteSettingsCategoryFilter.FILTER_PUSH_NOTIFICATIONS);
            fragmentArguments.putString(WebsitePreferences.EXTRA_TITLE,
                    applicationContext.getResources().getString(
                            R.string.push_notifications_permission_title));
        }
        preferencesIntent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArguments);

        // When coming from the gear on a flipped notification, we need to ensure that no existing
        // preference tasks are being re-used in order for it to appear on top.
        if (launchSingleWebsitePreferences)
            preferencesIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK);

        applicationContext.startActivity(preferencesIntent);
    }

    private PendingIntent getPendingIntent(String action, String notificationId, int platformId,
                                           byte[] notificationData) {
        Intent intent = new Intent(action);
        intent.setClass(mAppContext, NotificationService.Receiver.class);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_ID, notificationId);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_PLATFORM_ID, platformId);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_DATA, notificationData);

        return PendingIntent.getBroadcast(mAppContext, platformId, intent,
                PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private static String makePlatformTag(String tag, int platformId, String origin) {
        // The given tag may contain the separator character, so add it last to make reading the
        // preceding origin token reliable. If no tag was specified (it is the default empty
        // string), make the platform tag unique by appending the platform id.
        String platformTag = origin + NotificationConstants.NOTIFICATION_TAG_SEPARATOR + tag;
        if (TextUtils.isEmpty(tag)) platformTag += platformId;
        return platformTag;
    }

    /**
     * Displays a notification with the given |notificationId|, |title|, |body| and |icon|.
     *
     * @param tag A string identifier for this notification.
     * @param notificationId Unique id provided by the Chrome Notification system.
     * @param title Title to be displayed in the notification.
     * @param body Message to be displayed in the notification. Will be trimmed to one line of
     *             text by the Android notification system.
     * @param icon Icon to be displayed in the notification. When this isn't a valid Bitmap, a
     *             default icon will be generated instead.
     * @param origin Full text of the origin, including the protocol, owning this notification.
     * @param silent Whether the default sound, vibration and lights should be suppressed.
     * @param notificationData Serialized data associated with the notification.
     * @return The id using which the notification can be identified.
     */
    @CalledByNative
    private int displayNotification(String tag, String notificationId, String title, String body,
            Bitmap icon, String origin, boolean silent, byte[] notificationData) {
        if (icon == null || icon.getWidth() == 0) {
            icon = getIconGenerator().generateIconForUrl(origin, true);
        }

        Resources res = mAppContext.getResources();

        // Set up a pending intent for going to the settings screen for |origin|.
        Intent settingsIntent = PreferencesLauncher.createIntentForSettingsPage(
                mAppContext, SingleWebsitePreferences.class.getName());
        settingsIntent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS,
                SingleWebsitePreferences.createFragmentArgsForSite(origin));
        PendingIntent pendingSettingsIntent = PendingIntent.getActivity(
                mAppContext, 0, settingsIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        NotificationCompat.Builder notificationBuilder = new NotificationCompat.Builder(mAppContext)
                .setContentTitle(title)
                .setContentText(body)
                .setStyle(new NotificationCompat.BigTextStyle().bigText(body))
                .setLargeIcon(icon)
                .setSmallIcon(R.drawable.notification_badge)
                .setContentIntent(getPendingIntent(
                        NotificationConstants.ACTION_CLICK_NOTIFICATION,
                        notificationId, mLastNotificationId, notificationData))
                .setDeleteIntent(getPendingIntent(
                        NotificationConstants.ACTION_CLOSE_NOTIFICATION,
                        notificationId, mLastNotificationId, notificationData))
                .addAction(R.drawable.settings_cog,
                           res.getString(R.string.page_info_site_settings_button),
                           pendingSettingsIntent)
                .setSubText(origin);

        // Use the system's default ringtone, vibration and indicator lights unless the notification
        // has been marked as being silent, for example because it's low priority.
        if (!silent) notificationBuilder.setDefaults(Notification.DEFAULT_ALL);

        String platformTag = makePlatformTag(tag, mLastNotificationId, origin);
        mNotificationManager.notify(platformTag, mLastNotificationId, notificationBuilder.build());

        return mLastNotificationId++;
    }

    /**
     * Ensures the existance of an icon generator, which is created lazily.
     *
     * @return The icon generator which can be used.
     */
    private RoundedIconGenerator getIconGenerator() {
        if (mIconGenerator == null) {
            mIconGenerator = createRoundedIconGenerator(mAppContext);
        }

        return mIconGenerator;
    }

    /**
     * Creates the rounded icon generator to use for notifications based on the dimensions
     * and resolution of the device we're running on.
     *
     * @param appContext The application context to retrieve resources from.
     * @return The newly created rounded icon generator.
     */
    @VisibleForTesting
    public static RoundedIconGenerator createRoundedIconGenerator(Context appContext) {
        Resources res = appContext.getResources();
        float density = res.getDisplayMetrics().density;

        int widthPx = res.getDimensionPixelSize(android.R.dimen.notification_large_icon_width);
        int heightPx =
                res.getDimensionPixelSize(android.R.dimen.notification_large_icon_height);

        return new RoundedIconGenerator(
                widthPx,
                heightPx,
                Math.min(widthPx, heightPx) / 2,
                NOTIFICATION_ICON_BG_COLOR,
                NOTIFICATION_TEXT_SIZE_DP * density);
    }

    /**
     * Closes the notification identified by |tag|, |platformId|, and |origin|.
     */
    @CalledByNative
    private void closeNotification(String tag, int platformId, String origin) {
        String platformTag = makePlatformTag(tag, platformId, origin);
        mNotificationManager.cancel(platformTag, platformId);
    }

    private boolean onNotificationClicked(String notificationId, int platformId,
                                          byte[] notificationData) {
        return nativeOnNotificationClicked(
                mNativeNotificationManager, notificationId, platformId, notificationData);
    }

    private boolean onNotificationClosed(String notificationId) {
        return nativeOnNotificationClosed(mNativeNotificationManager, notificationId);
    }

    private static native void nativeInitializeNotificationUIManager();

    private native boolean nativeOnNotificationClicked(
            long nativeNotificationUIManagerAndroid, String notificationId,
            int platformId, byte[] notificationData);
    private native boolean nativeOnNotificationClosed(
            long nativeNotificationUIManagerAndroid, String notificationId);
}
