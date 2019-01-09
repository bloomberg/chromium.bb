// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.SyncAndServicesPreferences;
import org.chromium.chrome.browser.signin.AccountManagementFragment;
import org.chromium.chrome.browser.sync.GoogleServiceAuthError.State;
import org.chromium.chrome.browser.sync.ui.PassphraseActivity;
import org.chromium.components.sync.AndroidSyncSettings;

/**
 * {@link SyncNotificationController} provides functionality for displaying Android notifications
 * regarding the user sync status.
 */
public class SyncNotificationController implements ProfileSyncService.SyncStateChangedListener {
    private static final String TAG = "SyncNotificationController";
    private final NotificationManagerProxy mNotificationManager;
    private final ProfileSyncService mProfileSyncService;

    public SyncNotificationController() {
        mNotificationManager =
                new NotificationManagerProxyImpl(ContextUtils.getApplicationContext());
        mProfileSyncService = ProfileSyncService.get();
        assert mProfileSyncService != null;
    }

    /**
     * Callback for {@link ProfileSyncService.SyncStateChangedListener}.
     */
    @Override
    public void syncStateChanged() {
        ThreadUtils.assertOnUiThread();

        // Auth errors take precedence over passphrase errors.
        if (!AndroidSyncSettings.get().isSyncEnabled()) {
            mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_SYNC);
            return;
        }
        if (shouldSyncAuthErrorBeShown()) {
            showSyncNotification(
                    GoogleServiceAuthError.getMessageID(mProfileSyncService.getAuthError()),
                    createSettingsIntent());
        } else if (mProfileSyncService.isEngineInitialized()
                && mProfileSyncService.isPassphraseRequiredForDecryption()) {
            if (mProfileSyncService.isPassphrasePrompted()) {
                return;
            }
            switch (mProfileSyncService.getPassphraseType()) {
                case IMPLICIT_PASSPHRASE: // Falling through intentionally.
                case FROZEN_IMPLICIT_PASSPHRASE: // Falling through intentionally.
                case CUSTOM_PASSPHRASE:
                    showSyncNotification(R.string.sync_need_passphrase, createPasswordIntent());
                    break;
                case KEYSTORE_PASSPHRASE: // Falling through intentionally.
                default:
                    mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_SYNC);
                    return;
            }
        } else {
            mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_SYNC);
            return;
        }
    }

    /**
     * Builds and shows a notification for the |message|.
     *
     * @param message Resource id of the message to display in the notification.
     * @param intent Intent to send when the user activates the notification.
     */
    private void showSyncNotification(int message, Intent intent) {
        Context applicationContext = ContextUtils.getApplicationContext();
        String title = applicationContext.getString(R.string.app_name);
        String text = applicationContext.getString(R.string.sign_in_sync) + ": "
                + applicationContext.getString(message);

        PendingIntent contentIntent = PendingIntent.getActivity(applicationContext, 0, intent, 0);

        // There is no need to provide a group summary notification because the NOTIFICATION_ID_SYNC
        // notification id ensures there's only one sync notification at a time.
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(
                                true /* preferCompat */, ChannelDefinitions.ChannelId.BROWSER)
                        .setAutoCancel(true)
                        .setContentIntent(contentIntent)
                        .setContentTitle(title)
                        .setContentText(text)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setTicker(text)
                        .setLocalOnly(true)
                        .setGroup(NotificationConstants.GROUP_SYNC);

        Notification notification = builder.buildWithBigTextStyle(text);

        mNotificationManager.notify(NotificationConstants.NOTIFICATION_ID_SYNC, notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.SYNC, notification);
    }

    private boolean shouldSyncAuthErrorBeShown() {
        switch (mProfileSyncService.getAuthError()) {
            case State.NONE:
            case State.CONNECTION_FAILED:
            case State.SERVICE_UNAVAILABLE:
            case State.REQUEST_CANCELED:
            case State.INVALID_GAIA_CREDENTIALS:
                return false;
            case State.USER_NOT_SIGNED_UP:
            case State.CAPTCHA_REQUIRED:
            case State.ACCOUNT_DELETED:
            case State.ACCOUNT_DISABLED:
            case State.TWO_FACTOR:
                return true;
            default:
                Log.w(TAG, "Not showing unknown Auth Error: " + mProfileSyncService.getAuthError());
                return false;
        }
    }

    /**
     * Creates an intent that launches the Chrome settings, and automatically opens the fragment
     * for signed in users.
     *
     * @return the intent for opening the settings
     */
    private Intent createSettingsIntent() {
        final String fragmentName;
        final Bundle fragmentArguments;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UNIFIED_CONSENT)) {
            fragmentName = SyncAndServicesPreferences.class.getName();
            fragmentArguments = SyncAndServicesPreferences.createArguments(false);
        } else {
            fragmentName = AccountManagementFragment.class.getName();
            fragmentArguments = null;
        }
        return PreferencesLauncher.createIntentForSettingsPage(
                ContextUtils.getApplicationContext(), fragmentName, fragmentArguments);
    }

    /**
     * Creates an intent that launches an activity that requests the users password/passphrase.
     *
     * @return the intent for opening the password/passphrase activity
     */
    private Intent createPasswordIntent() {
        // Make sure we don't prompt too many times.
        mProfileSyncService.setPassphrasePrompted(true);

        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setComponent(
                new ComponentName(ContextUtils.getApplicationContext(), PassphraseActivity.class));
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        // This activity will become the start of a new task on this history stack.
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Clears the task stack above this activity if it already exists.
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return intent;
    }
}
