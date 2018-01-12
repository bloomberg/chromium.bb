// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.RemoteInput;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.style.StyleSpan;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.website.SingleCategoryPreferences;
import org.chromium.chrome.browser.preferences.website.SingleWebsitePreferences;
import org.chromium.chrome.browser.preferences.website.SiteSettingsCategory;
import org.chromium.chrome.browser.webapps.ChromeWebApkHost;
import org.chromium.chrome.browser.webapps.WebApkServiceClient;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.webapk.lib.client.WebApkIdentityServiceClient;
import org.chromium.webapk.lib.client.WebApkValidator;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.concurrent.TimeUnit;

import javax.annotation.Nullable;

/**
 * Provides the ability for the NotificationPlatformBridgeAndroid to talk to the Android platform
 * notification system.
 *
 * This class should only be used on the UI thread.
 */
public class NotificationPlatformBridge {
    private static final String TAG = NotificationPlatformBridge.class.getSimpleName();

    // We always use the same integer id when showing and closing notifications. The notification
    // tag is always set, which is a safe and sufficient way of identifying a notification, so the
    // integer id is not needed anymore except it must not vary in an uncontrolled way.
    @VisibleForTesting static final int PLATFORM_ID = -1;

    // We always use the same request code for pending intents. We use other ways to force
    // uniqueness of pending intents when necessary.
    private static final int PENDING_INTENT_REQUEST_CODE = 0;

    private static final int[] EMPTY_VIBRATION_PATTERN = new int[0];

    private static NotificationPlatformBridge sInstance;

    private static NotificationManagerProxy sNotificationManagerOverride;

    private final long mNativeNotificationPlatformBridge;

    private final NotificationManagerProxy mNotificationManager;

    private long mLastNotificationClickMs;

    /**
     * Creates a new instance of the NotificationPlatformBridge.
     *
     * @param nativeNotificationPlatformBridge Instance of the NotificationPlatformBridgeAndroid
     *        class.
     */
    @CalledByNative
    private static NotificationPlatformBridge create(long nativeNotificationPlatformBridge) {
        if (sInstance != null) {
            throw new IllegalStateException(
                "There must only be a single NotificationPlatformBridge.");
        }

        sInstance = new NotificationPlatformBridge(nativeNotificationPlatformBridge);
        return sInstance;
    }

    /**
     * Returns the current instance of the NotificationPlatformBridge.
     *
     * @return The instance of the NotificationPlatformBridge, if any.
     */
    @Nullable
    @VisibleForTesting
    static NotificationPlatformBridge getInstanceForTests() {
        return sInstance;
    }

    /**
     * Overrides the notification manager which is to be used for displaying Notifications on the
     * Android framework. Should only be used for testing. Tests are expected to clean up after
     * themselves by setting this to NULL again.
     *
     * @param notificationManager The notification manager instance to use instead of the system's.
     */
    @VisibleForTesting
    static void overrideNotificationManagerForTesting(
            NotificationManagerProxy notificationManager) {
        sNotificationManagerOverride = notificationManager;
    }

    private NotificationPlatformBridge(long nativeNotificationPlatformBridge) {
        mNativeNotificationPlatformBridge = nativeNotificationPlatformBridge;
        Context context = ContextUtils.getApplicationContext();
        if (sNotificationManagerOverride != null) {
            mNotificationManager = sNotificationManagerOverride;
        } else {
            mNotificationManager = new NotificationManagerProxyImpl(
                    (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE));
        }
    }

    /**
     * Marks the current instance as being freed, allowing for a new NotificationPlatformBridge
     * object to be initialized.
     */
    @CalledByNative
    private void destroy() {
        assert sInstance == this;
        sInstance = null;
    }

    /**
     * Invoked by the NotificationService when a Notification intent has been received. There may
     * not be an active instance of the NotificationPlatformBridge at this time, so inform the
     * native side through a static method, initializing both ends if needed.
     *
     * @param intent The intent as received by the Notification service.
     * @return Whether the event could be handled by the native Notification bridge.
     */
    static boolean dispatchNotificationEvent(Intent intent) {
        if (sInstance == null) {
            nativeInitializeNotificationPlatformBridge();
            if (sInstance == null) {
                Log.e(TAG, "Unable to initialize the native NotificationPlatformBridge.");
                return false;
            }
        }
        recordJobStartDelayUMA(intent);

        String notificationId = intent.getStringExtra(NotificationConstants.EXTRA_NOTIFICATION_ID);

        String origin = intent.getStringExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_ORIGIN);
        String scopeUrl =
                intent.getStringExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_SCOPE);
        if (scopeUrl == null) scopeUrl = "";
        String profileId =
                intent.getStringExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_ID);
        boolean incognito = intent.getBooleanExtra(
                NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_INCOGNITO, false);
        String tag = intent.getStringExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_TAG);

        Log.i(TAG, "Dispatching notification event to native: " + notificationId);

        if (NotificationConstants.ACTION_CLICK_NOTIFICATION.equals(intent.getAction())) {
            String webApkPackage = intent.getStringExtra(
                    NotificationConstants.EXTRA_NOTIFICATION_INFO_WEBAPK_PACKAGE);
            if (webApkPackage == null) {
                webApkPackage = "";
            }
            int actionIndex = intent.getIntExtra(
                    NotificationConstants.EXTRA_NOTIFICATION_INFO_ACTION_INDEX, -1);
            sInstance.onNotificationClicked(notificationId, origin, scopeUrl, profileId, incognito,
                    tag, webApkPackage, actionIndex, getNotificationReply(intent));
            return true;
        } else if (NotificationConstants.ACTION_CLOSE_NOTIFICATION.equals(intent.getAction())) {
            // Notification deleteIntent is executed only "when the notification is explicitly
            // dismissed by the user, either with the 'Clear All' button or by swiping it away
            // individually" (though a third-party NotificationListenerService may also trigger it).
            sInstance.onNotificationClosed(
                    notificationId, origin, profileId, incognito, tag, true /* byUser */);
            return true;
        }

        Log.e(TAG, "Unrecognized Notification action: " + intent.getAction());
        return false;
    }

    private static void recordJobStartDelayUMA(Intent intent) {
        if (intent.hasExtra(NotificationConstants.EXTRA_JOB_SCHEDULED_TIME_MS)
                && intent.hasExtra(NotificationConstants.EXTRA_JOB_STARTED_TIME_MS)) {
            long duration = intent.getLongExtra(NotificationConstants.EXTRA_JOB_STARTED_TIME_MS, -1)
                    - intent.getLongExtra(NotificationConstants.EXTRA_JOB_SCHEDULED_TIME_MS, -1);
            if (duration < 0) return; // Possible if device rebooted before job started.
            RecordHistogram.recordMediumTimesHistogram(
                    "Notifications.Android.JobStartDelay", duration, TimeUnit.MILLISECONDS);
        }
    }

    @Nullable
    static String getNotificationReply(Intent intent) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            // RemoteInput was added in KITKAT_WATCH.
            if (intent.getStringExtra(NotificationConstants.EXTRA_NOTIFICATION_REPLY) != null) {
                // If the notification click went through the job scheduler, we will have set
                // the reply as a standard string extra.
                return intent.getStringExtra(NotificationConstants.EXTRA_NOTIFICATION_REPLY);
            }
            Bundle remoteInputResults = RemoteInput.getResultsFromIntent(intent);
            if (remoteInputResults != null) {
                CharSequence reply =
                        remoteInputResults.getCharSequence(NotificationConstants.KEY_TEXT_REPLY);
                if (reply != null) {
                    return reply.toString();
                }
            }
        }
        return null;
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
        // This method handles an intent fired by the Android system. There is no guarantee that the
        // native library is loaded at this point. The native library is needed for the preferences
        // activity, and it loads the library, but there are some native calls even before that
        // activity is started: from RecordUserAction.record and (indirectly) from
        // UrlFormatter.formatUrlForSecurityDisplay.
        try {
            ChromeBrowserInitializer.getInstance(context).handleSynchronousStartup();
        } catch (ProcessInitException e) {
            Log.e(TAG, "Failed to start browser process.", e);
            // The library failed to initialize and nothing in the application can work, so kill
            // the whole application.
            System.exit(-1);
            return;
        }

        // Use the application context because it lives longer. When using the given context, it
        // may be stopped before the preferences intent is handled.
        Context applicationContext = context.getApplicationContext();

        // If we can read an origin from the intent, use it to open the settings screen for that
        // origin.
        String origin = getOriginFromIntent(incomingIntent);
        boolean launchSingleWebsitePreferences = origin != null;

        String fragmentName = launchSingleWebsitePreferences
                ? SingleWebsitePreferences.class.getName()
                : SingleCategoryPreferences.class.getName();
        Intent preferencesIntent =
                PreferencesLauncher.createIntentForSettingsPage(applicationContext, fragmentName);

        Bundle fragmentArguments;
        if (launchSingleWebsitePreferences) {
            // Record that the user has clicked on the [Site Settings] button.
            RecordUserAction.record("Notifications.ShowSiteSettings");

            // All preferences for a specific origin.
            fragmentArguments = SingleWebsitePreferences.createFragmentArgsForSite(origin);
        } else {
            // Notification preferences for all origins.
            fragmentArguments = new Bundle();
            fragmentArguments.putString(SingleCategoryPreferences.EXTRA_CATEGORY,
                    SiteSettingsCategory.CATEGORY_NOTIFICATIONS);
            fragmentArguments.putString(SingleCategoryPreferences.EXTRA_TITLE,
                    applicationContext.getResources().getString(
                            R.string.push_notifications_permission_title));
        }
        preferencesIntent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArguments);

        // We need to ensure that no existing preference tasks are being re-used in order for the
        // new activity to appear on top.
        preferencesIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK);

        applicationContext.startActivity(preferencesIntent);
    }

    /**
     * Returns a bogus Uri used to make each intent unique according to Intent#filterEquals.
     * Without this, the pending intents derived from the intent may be reused, because extras are
     * not taken into account for the filterEquals comparison.
     *
     * @param notificationId The id of the notification.
     * @param origin The origin to whom the notification belongs.
     * @param actionIndex The zero-based index of the action button, or -1 if not applicable.
     */
    private Uri makeIntentData(String notificationId, String origin, int actionIndex) {
        return Uri.parse(origin).buildUpon().fragment(notificationId + "," + actionIndex).build();
    }

    /**
     * Returns the PendingIntent for completing |action| on the notification identified by the data
     * in the other parameters.
     *
     * All parameters set here should also be set in
     * {@link NotificationJobService#getJobExtrasFromIntent(Intent)}.
     *
     * @param context An appropriate context for the intent class and broadcast.
     * @param action The action this pending intent will represent.
     * @param notificationId The id of the notification.
     * @param origin The origin to whom the notification belongs.
     * @param scopeUrl The scope of the service worker registered by the site where the notification
     *                 comes from.
     * @param profileId Id of the profile to which the notification belongs.
     * @param incognito Whether the profile was in incognito mode.
     * @param tag The tag of the notification. May be NULL.
     * @param webApkPackage The package of the WebAPK associated with the notification. Empty if
     *        the notification is not associated with a WebAPK.
     * @param actionIndex The zero-based index of the action button, or -1 if not applicable.
     */
    private PendingIntent makePendingIntent(Context context, String action, String notificationId,
            String origin, String scopeUrl, String profileId, boolean incognito,
            @Nullable String tag, String webApkPackage, int actionIndex) {
        Uri intentData = makeIntentData(notificationId, origin, actionIndex);
        Intent intent = new Intent(action, intentData);
        intent.setClass(context, NotificationService.Receiver.class);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_ID, notificationId);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_ORIGIN, origin);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_SCOPE, scopeUrl);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_ID, profileId);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_PROFILE_INCOGNITO, incognito);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_TAG, tag);
        intent.putExtra(
                NotificationConstants.EXTRA_NOTIFICATION_INFO_WEBAPK_PACKAGE, webApkPackage);
        intent.putExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_ACTION_INDEX, actionIndex);

        // This flag ensures the broadcast is delivered with foreground priority. It also means the
        // receiver gets a shorter timeout interval before it may be killed, but this is ok because
        // we schedule a job to handle the intent in NotificationService.Receiver on N+.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            intent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        }

        return PendingIntent.getBroadcast(
                context, PENDING_INTENT_REQUEST_CODE, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    /**
     * Attempts to extract an origin from the tag extras in the given intent.
     *
     * There are two tags that are relevant, either or none of them may be set, but not both:
     *   1. Notification.EXTRA_CHANNEL_ID - set by Android on the 'Additional settings in the app'
     *      button intent from individual channel settings screens in Android O.
     *   2. NotificationConstants.EXTRA_NOTIFICATION_TAG - set by us on browser UI that should
     *     launch specific site settings, e.g. the web notifications Site Settings button.
     *
     * See {@link SiteChannelsManager#createChannelId} and {@link SiteChannelsManager#toSiteOrigin}
     * for how we convert origins to and from channel ids.
     *
     * @param intent The incoming intent.
     * @return The origin string. Returns null if there was no relevant tag extra in the given
     * intent, or if a relevant notification tag value did not match the expected format.
     */
    @Nullable
    private static String getOriginFromIntent(Intent intent) {
        String originFromChannelId =
                getOriginFromChannelId(intent.getStringExtra(Notification.EXTRA_CHANNEL_ID));
        return originFromChannelId != null ? originFromChannelId
                                           : getOriginFromNotificationTag(intent.getStringExtra(
                                                     NotificationConstants.EXTRA_NOTIFICATION_TAG));
    }

    /**
     * Gets origin from the notification tag.
     * If the user touched the settings cog on a flipped notification originating from this
     * class, there will be a notification tag extra in a specific format. From the tag we can
     * read the origin of the notification.
     *
     * @param tag The notification tag to extract origin from.
     * @return The origin string. Return null if there was no tag extra in the given notification
     * tag, or if the notification tag didn't match the expected format.
     */
    @Nullable
    @VisibleForTesting
    static String getOriginFromNotificationTag(@Nullable String tag) {
        if (tag == null
                || !tag.startsWith(NotificationConstants.PERSISTENT_NOTIFICATION_TAG_PREFIX
                           + NotificationConstants.NOTIFICATION_TAG_SEPARATOR))
            return null;

        // This code parses the notification id that was generated in notification_id_generator.cc
        // TODO(https://crbug.com/801164): Extract this to a separate class.
        String[] parts = tag.split(NotificationConstants.NOTIFICATION_TAG_SEPARATOR);
        assert parts.length >= 3;
        try {
            URI uri = new URI(parts[1]);
            if (uri.getHost() != null) return parts[1];
        } catch (URISyntaxException e) {
            Log.e(TAG, "Expected to find a valid url in the notification tag extra.", e);
            return null;
        }
        return null;
    }

    @Nullable
    @VisibleForTesting
    static String getOriginFromChannelId(@Nullable String channelId) {
        if (channelId == null
                || !channelId.startsWith(ChannelDefinitions.CHANNEL_ID_PREFIX_SITES)) {
            return null;
        }
        return SiteChannelsManager.toSiteOrigin(channelId);
    }

    /**
     * Generates the notification defaults from vibrationPattern's size and silent.
     *
     * Use the system's default ringtone, vibration and indicator lights unless the notification
     * has been marked as being silent.
     * If a vibration pattern is set, the notification should use the provided pattern
     * rather than defaulting to the system settings.
     *
     * @param vibrationPatternLength Vibration pattern's size for the Notification.
     * @param silent Whether the default sound, vibration and lights should be suppressed.
     * @param vibrateEnabled Whether vibration is enabled in preferences.
     * @return The generated notification's default value.
    */
    @VisibleForTesting
    static int makeDefaults(int vibrationPatternLength, boolean silent, boolean vibrateEnabled) {
        assert !silent || vibrationPatternLength == 0;

        if (silent) return 0;

        int defaults = Notification.DEFAULT_ALL;
        if (vibrationPatternLength > 0 || !vibrateEnabled) {
            defaults &= ~Notification.DEFAULT_VIBRATE;
        }
        return defaults;
    }

    /**
     * Generates the vibration pattern used in Android notification.
     *
     * Android takes a long array where the first entry indicates the number of milliseconds to wait
     * prior to starting the vibration, whereas Chrome follows the syntax of the Web Vibration API.
     *
     * @param vibrationPattern Vibration pattern following the Web Vibration API syntax.
     * @return Vibration pattern following the Android syntax.
    */
    @VisibleForTesting
    static long[] makeVibrationPattern(int[] vibrationPattern) {
        long[] pattern = new long[vibrationPattern.length + 1];
        for (int i = 0; i < vibrationPattern.length; ++i) {
            pattern[i + 1] = vibrationPattern[i];
        }
        return pattern;
    }

    /**
     * Displays a notification with the given details.
     *
     * @param notificationId The id of the notification.
     * @param origin Full text of the origin, including the protocol, owning this notification.
     * @param scopeUrl The scope of the service worker registered by the site where the notification
     *                 comes from.
     * @param profileId Id of the profile that showed the notification.
     * @param incognito if the session of the profile is an off the record one.
     * @param tag A string identifier for this notification. If the tag is not empty, the new
     *            notification will replace the previous notification with the same tag and origin,
     *            if present. If no matching previous notification is present, the new one will just
     *            be added.
     * @param title Title to be displayed in the notification.
     * @param body Message to be displayed in the notification. Will be trimmed to one line of
     *             text by the Android notification system.
     * @param image Content image to be prominently displayed when the notification is expanded.
     * @param icon Icon to be displayed in the notification. Valid Bitmap icons will be scaled to
     *             the platforms, whereas a default icon will be generated for invalid Bitmaps.
     * @param badge An image to represent the notification in the status bar. It is also displayed
     *              inside the notification.
     * @param vibrationPattern Vibration pattern following the Web Vibration syntax.
     * @param timestamp The timestamp of the event for which the notification is being shown.
     * @param renotify Whether the sound, vibration, and lights should be replayed if the
     *                 notification is replacing another notification.
     * @param silent Whether the default sound, vibration and lights should be suppressed.
     * @param actions Action buttons to display alongside the notification.
     * @see <a href="https://developer.android.com/reference/android/app/Notification.html">
     *     Android Notification API</a>
     */
    @CalledByNative
    private void displayNotification(final String notificationId, final String origin,
            final String scopeUrl, final String profileId, final boolean incognito,
            final String tag, final String title, final String body, final Bitmap image,
            final Bitmap icon, final Bitmap badge, final int[] vibrationPattern,
            final long timestamp, final boolean renotify, final boolean silent,
            final ActionInfo[] actions) {
        final String webApkPackage =
                WebApkValidator.queryWebApkPackage(ContextUtils.getApplicationContext(), scopeUrl);
        if (webApkPackage != null) {
            WebApkIdentityServiceClient.CheckBrowserBacksWebApkCallback callback =
                    new WebApkIdentityServiceClient.CheckBrowserBacksWebApkCallback() {
                        @Override
                        public void onChecked(boolean doesBrowserBackWebApk) {
                            displayNotificationInternal(notificationId, origin, scopeUrl, profileId,
                                    incognito, tag, title, body, image, icon, badge,
                                    vibrationPattern, timestamp, renotify, silent, actions,
                                    doesBrowserBackWebApk ? webApkPackage : "");
                        }
                    };
            ChromeWebApkHost.checkChromeBacksWebApkAsync(webApkPackage, callback);
            return;
        }

        displayNotificationInternal(notificationId, origin, scopeUrl, profileId, incognito, tag,
                title, body, image, icon, badge, vibrationPattern, timestamp, renotify, silent,
                actions, "");
    }

    /** Called after querying whether the browser backs the given WebAPK. */
    private void displayNotificationInternal(String notificationId, String origin, String scopeUrl,
            String profileId, boolean incognito, String tag, String title, String body,
            Bitmap image, Bitmap icon, Bitmap badge, int[] vibrationPattern, long timestamp,
            boolean renotify, boolean silent, ActionInfo[] actions, String webApkPackage) {
        nativeStoreCachedWebApkPackageForNotificationId(
                mNativeNotificationPlatformBridge, notificationId, webApkPackage);

        Context context = ContextUtils.getApplicationContext();
        Resources res = context.getResources();

        // Record whether it's known whether notifications can be shown to the user at all.
        RecordHistogram.recordEnumeratedHistogram("Notifications.AppNotificationStatus",
                NotificationSystemStatusUtil.determineAppNotificationStatus(context),
                NotificationSystemStatusUtil.APP_NOTIFICATIONS_STATUS_BOUNDARY);

        PendingIntent clickIntent = makePendingIntent(context,
                NotificationConstants.ACTION_CLICK_NOTIFICATION, notificationId, origin, scopeUrl,
                profileId, incognito, tag, webApkPackage, -1 /* actionIndex */);
        PendingIntent closeIntent = makePendingIntent(context,
                NotificationConstants.ACTION_CLOSE_NOTIFICATION, notificationId, origin, scopeUrl,
                profileId, incognito, tag, webApkPackage, -1 /* actionIndex */);

        boolean hasImage = image != null;
        boolean forWebApk = !webApkPackage.isEmpty();
        NotificationBuilderBase notificationBuilder =
                createNotificationBuilder(context, hasImage)
                        .setTitle(title)
                        .setBody(body)
                        .setImage(image)
                        .setLargeIcon(icon)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setSmallIcon(badge)
                        .setContentIntent(clickIntent)
                        .setDeleteIntent(closeIntent)
                        .setTicker(createTickerText(title, body))
                        .setTimestamp(timestamp)
                        .setRenotify(renotify)
                        .setOrigin(UrlFormatter.formatUrlForSecurityDisplay(
                                origin, false /* showScheme */));

        if (shouldSetChannelId(forWebApk)) {
            // TODO(crbug.com/700377): Channel ID should be retrieved from cache in native and
            // passed through to here with other notification parameters.
            String channelId =
                    ChromeFeatureList.isEnabled(ChromeFeatureList.SITE_NOTIFICATION_CHANNELS)
                    ? SiteChannelsManager.getInstance().getChannelIdForOrigin(origin)
                    : ChannelDefinitions.CHANNEL_ID_SITES;
            notificationBuilder.setChannelId(channelId);
        }

        for (int actionIndex = 0; actionIndex < actions.length; actionIndex++) {
            PendingIntent intent = makePendingIntent(context,
                    NotificationConstants.ACTION_CLICK_NOTIFICATION, notificationId, origin,
                    scopeUrl, profileId, incognito, tag, webApkPackage, actionIndex);
            ActionInfo action = actions[actionIndex];
            // Don't show action button icons when there's an image, as then action buttons go on
            // the same row as the Site Settings button, so icons wouldn't leave room for text.
            Bitmap actionIcon = hasImage ? null : action.icon;
            if (action.type == NotificationActionType.TEXT) {
                notificationBuilder.addTextAction(
                        actionIcon, action.title, intent, action.placeholder);
            } else {
                notificationBuilder.addButtonAction(actionIcon, action.title, intent);
            }
        }

        // The Android framework applies a fallback vibration pattern for the sound when the device
        // is in vibrate mode, there is no custom pattern, and the vibration default has been
        // disabled. To truly prevent vibration, provide a custom empty pattern.
        boolean vibrateEnabled = PrefServiceBridge.getInstance().isNotificationsVibrateEnabled();
        if (!vibrateEnabled) {
            vibrationPattern = EMPTY_VIBRATION_PATTERN;
        }
        notificationBuilder.setDefaults(
                makeDefaults(vibrationPattern.length, silent, vibrateEnabled));
        notificationBuilder.setVibrate(makeVibrationPattern(vibrationPattern));

        if (forWebApk) {
            WebApkServiceClient.getInstance().notifyNotification(
                    webApkPackage, notificationBuilder, notificationId, PLATFORM_ID);
        } else {
            // Set up a pending intent for going to the settings screen for |origin|.
            Intent settingsIntent = PreferencesLauncher.createIntentForSettingsPage(
                    context, SingleWebsitePreferences.class.getName());
            settingsIntent.setData(makeIntentData(notificationId, origin, -1 /* actionIndex */));
            settingsIntent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS,
                    SingleWebsitePreferences.createFragmentArgsForSite(origin));

            PendingIntent pendingSettingsIntent = PendingIntent.getActivity(context,
                    PENDING_INTENT_REQUEST_CODE, settingsIntent, PendingIntent.FLAG_UPDATE_CURRENT);

            // If action buttons are displayed, there isn't room for the full Site Settings button
            // label and icon, so abbreviate it. This has the unfortunate side-effect of
            // unnecessarily abbreviating it on Android Wear also (crbug.com/576656). If custom
            // layouts are enabled, the label and icon provided here only affect Android Wear, so
            // don't abbreviate them.
            boolean abbreviateSiteSettings = actions.length > 0 && !useCustomLayouts(hasImage);
            int settingsIconId = abbreviateSiteSettings ? 0 : R.drawable.settings_cog;
            CharSequence settingsTitle = abbreviateSiteSettings
                    ? res.getString(R.string.notification_site_settings_button)
                    : res.getString(R.string.page_info_site_settings_button);
            // If the settings button is displayed together with the other buttons it has to be the
            // last one, so add it after the other actions.
            notificationBuilder.addSettingsAction(
                    settingsIconId, settingsTitle, pendingSettingsIntent);

            mNotificationManager.notify(notificationId, PLATFORM_ID, notificationBuilder.build());
            NotificationUmaTracker.getInstance().onNotificationShown(
                    NotificationUmaTracker.SITES, notificationBuilder.mChannelId);
        }
    }

    private NotificationBuilderBase createNotificationBuilder(Context context, boolean hasImage) {
        if (useCustomLayouts(hasImage)) {
            return new CustomNotificationBuilder(context);
        }
        return new StandardNotificationBuilder(context);
    }

    /** Returns whether to set a channel id when building a notification. */
    private boolean shouldSetChannelId(boolean forWebApk) {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && !forWebApk;
    }

    /**
     * Creates the ticker text for a notification having |title| and |body|. The notification's
     * title will be printed in bold, followed by the text of the body.
     *
     * @param title Title of the notification.
     * @param body Textual contents of the notification.
     * @return A character sequence containing the ticker's text.
     */
    private CharSequence createTickerText(String title, String body) {
        SpannableStringBuilder spannableStringBuilder = new SpannableStringBuilder();

        spannableStringBuilder.append(title);
        spannableStringBuilder.append("\n");
        spannableStringBuilder.append(body);

        // Mark the title of the notification as being bold.
        spannableStringBuilder.setSpan(new StyleSpan(android.graphics.Typeface.BOLD),
                0, title.length(), Spannable.SPAN_INCLUSIVE_INCLUSIVE);

        return spannableStringBuilder;
    }

    /**
     * Determines whether to use standard notification layouts, using NotificationCompat.Builder,
     * or custom layouts using Chrome's own templates.
     *
     * Normally a standard layout is used on Android N+, and a custom layout is used on older
     * versions of Android. But if the notification has a content image, there isn't enough room for
     * the Site Settings button to go on its own line when showing an image, nor is there enough
     * room for action button icons, so a standard layout will be used here even on old versions.
     *
     * @param hasImage Whether the notification has a content image.
     * @return Whether custom layouts should be used.
     */
    @VisibleForTesting
    static boolean useCustomLayouts(boolean hasImage) {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.N && !hasImage;
    }

    /**
     * Returns whether a notification has been clicked in the last 5 seconds.
     * Used for Startup.BringToForegroundReason UMA histogram.
     */
    public static boolean wasNotificationRecentlyClicked() {
        if (sInstance == null) return false;
        long now = System.currentTimeMillis();
        return now - sInstance.mLastNotificationClickMs < 5 * 1000;
    }

    /**
     * Closes the notification associated with the given parameters.
     *
     * @param notificationId The id of the notification.
     * @param scopeUrl The scope of the service worker registered by the site where the notification
     *                 comes from.
     * @param hasQueriedWebApkPackage Whether has done the query of is there a WebAPK can handle
     *                                this notification.
     * @param webApkPackage The package of the WebAPK associated with the notification.
     *                      Empty if the notification is not associated with a WebAPK.
     */
    @CalledByNative
    private void closeNotification(final String notificationId, String scopeUrl,
            boolean hasQueriedWebApkPackage, String webApkPackage) {
        if (!hasQueriedWebApkPackage) {
            final String webApkPackageFound = WebApkValidator.queryWebApkPackage(
                    ContextUtils.getApplicationContext(), scopeUrl);
            if (webApkPackageFound != null) {
                WebApkIdentityServiceClient.CheckBrowserBacksWebApkCallback callback =
                        new WebApkIdentityServiceClient.CheckBrowserBacksWebApkCallback() {
                            @Override
                            public void onChecked(boolean doesBrowserBackWebApk) {
                                closeNotificationInternal(notificationId,
                                        doesBrowserBackWebApk ? webApkPackageFound : null);
                            }
                        };
                ChromeWebApkHost.checkChromeBacksWebApkAsync(webApkPackageFound, callback);
                return;
            }
        }
        closeNotificationInternal(notificationId, webApkPackage);
    }

    /** Called after querying whether the browser backs the given WebAPK. */
    private void closeNotificationInternal(String notificationId, String webApkPackage) {
        if (TextUtils.isEmpty(webApkPackage)) {
            mNotificationManager.cancel(notificationId, PLATFORM_ID);
        } else {
            WebApkServiceClient.getInstance().cancelNotification(
                    webApkPackage, notificationId, PLATFORM_ID);
        }
    }

    /**
     * Calls NotificationPlatformBridgeAndroid::OnNotificationClicked in native code to indicate
     * that the notification with the given parameters has been clicked on.
     *
     * @param notificationId The id of the notification.
     * @param origin The origin of the notification.
     * @param scopeUrl The scope of the service worker registered by the site where the notification
     *                 comes from.
     * @param profileId Id of the profile that showed the notification.
     * @param incognito if the profile session was an off the record one.
     * @param tag The tag of the notification. May be NULL.
     * @param webApkPackage The package of the WebAPK associated with the notification.
     *                      Empty if the notification is not associated with a WebAPK.
     * @param actionIndex The index of the action button that was clicked, or -1 if not applicable.
     * @param reply User reply to a text action on the notification. Null if the user did not click
     *              on a text action or if inline replies are not supported.
     */
    private void onNotificationClicked(String notificationId, String origin, String scopeUrl,
            String profileId, boolean incognito, String tag, String webApkPackage, int actionIndex,
            @Nullable String reply) {
        mLastNotificationClickMs = System.currentTimeMillis();
        nativeOnNotificationClicked(mNativeNotificationPlatformBridge, notificationId, origin,
                scopeUrl, profileId, incognito, tag, webApkPackage, actionIndex, reply);
    }

    /**
     * Calls NotificationPlatformBridgeAndroid::OnNotificationClosed in native code to indicate that
     * the notification with the given parameters has been closed.
     *
     * @param notificationId The id of the notification.
     * @param origin The origin of the notification.
     * @param profileId Id of the profile that showed the notification.
     * @param incognito if the profile session was an off the record one.
     * @param tag The tag of the notification. May be NULL.
     * @param byUser Whether the notification was closed by a user gesture.
     */
    private void onNotificationClosed(String notificationId, String origin, String profileId,
            boolean incognito, String tag, boolean byUser) {
        nativeOnNotificationClosed(mNativeNotificationPlatformBridge, notificationId, origin,
                profileId, incognito, tag, byUser);
    }

    private static native void nativeInitializeNotificationPlatformBridge();

    private native void nativeOnNotificationClicked(long nativeNotificationPlatformBridgeAndroid,
            String notificationId, String origin, String scopeUrl, String profileId,
            boolean incognito, String tag, String webApkPackage, int actionIndex, String reply);
    private native void nativeOnNotificationClosed(long nativeNotificationPlatformBridgeAndroid,
            String notificationId, String origin, String profileId, boolean incognito, String tag,
            boolean byUser);
    private native void nativeStoreCachedWebApkPackageForNotificationId(
            long nativeNotificationPlatformBridgeAndroid, String notificationId,
            String webApkPackage);
}
