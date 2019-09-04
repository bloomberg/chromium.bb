// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.click_to_call;

import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.DeviceConditions;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.sharing.SharingNotificationUtil;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;

/**
 * Manages ClickToCall related notifications for Android.
 */
public class ClickToCallMessageHandler {
    private static final String EXTRA_PHONE_NUMBER = "ClickToCallMessageHandler.EXTRA_PHONE_NUMBER";

    /**
     * Opens the dialer with the |phoneNumber| already prefilled.
     *
     * @param phoneNumber The phone number to show in the dialer.
     */
    private static void openDialer(String phoneNumber) {
        final Intent dialIntent;
        if (!TextUtils.isEmpty(phoneNumber)) {
            dialIntent = new Intent(Intent.ACTION_DIAL, Uri.parse("tel:" + phoneNumber));
        } else {
            dialIntent = new Intent(Intent.ACTION_DIAL);
        }
        dialIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        try {
            ContextUtils.getApplicationContext().startActivity(dialIntent);
            ClickToCallUma.recordDialerPresent(true);
            ClickToCallUma.recordDialerShown(TextUtils.isEmpty(phoneNumber));
        } catch (ActivityNotFoundException activityNotFound) {
            // TODO(crbug.com/996644): Add error dialog when no dialer app is available.
            ClickToCallUma.recordDialerPresent(false);
        }
    }

    /**
     * Handles the tapping of a notification by opening the dialer with the
     * phone number specified in the notification.
     */
    public static final class TapReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            openDialer(IntentUtils.safeGetStringExtra(intent, EXTRA_PHONE_NUMBER));
        }
    }

    /**
     * Handles the device unlock event to remove notification and just show dialer.
     */
    public static final class PhoneUnlockedReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            // We only try to remove the notifications if we have opened the dialer.
            if (intent != null && Intent.ACTION_USER_PRESENT.equals(intent.getAction())
                    && shouldOpenDialer()) {
                SharingNotificationUtil.dismissNotification(
                        NotificationConstants.GROUP_CLICK_TO_CALL,
                        NotificationConstants.NOTIFICATION_ID_CLICK_TO_CALL);
            }
        }
    }

    /**
     * Displays a notification that opens the dialer when clicked.
     *
     * @param phoneNumber The phone number to show in the dialer when the user taps the
     *                    notification.
     */
    private static void displayNotification(String phoneNumber) {
        Context context = ContextUtils.getApplicationContext();
        PendingIntentProvider contentIntent = PendingIntentProvider.getBroadcast(context,
                /*requestCode=*/0,
                new Intent(context, TapReceiver.class).putExtra(EXTRA_PHONE_NUMBER, phoneNumber),
                PendingIntent.FLAG_UPDATE_CURRENT);
        SharingNotificationUtil.showNotification(
                NotificationUmaTracker.SystemNotificationType.CLICK_TO_CALL,
                NotificationConstants.GROUP_CLICK_TO_CALL,
                NotificationConstants.NOTIFICATION_ID_CLICK_TO_CALL, contentIntent, phoneNumber,
                context.getResources().getString(R.string.click_to_call_notification_text),
                R.drawable.ic_dialer_icon_blue_40dp);
    }

    /**
     * Returns true if we should open the dialer straight away.
     */
    private static boolean shouldOpenDialer() {
        // On Q and above, we never open the dialer directly. On pre-Q, we always open the dialer
        // directly.
        return !BuildInfo.isAtLeastQ() && FeatureUtilities.isClickToCallOpenDialerDirectlyEnabled();
    }

    /**
     * Returns true if we should show notification to the user.
     */
    private static boolean shouldShowNotification() {
        if (!FeatureUtilities.isClickToCallOpenDialerDirectlyEnabled()) {
            return true;
        }

        // Always show the notification for Android Q and above. For pre-Q, only show notification
        // if device is locked.
        return BuildInfo.isAtLeastQ()
                || !DeviceConditions.isCurrentlyScreenOnAndUnlocked(
                        ContextUtils.getApplicationContext());
    }

    /**
     * Handles a phone number sent from another device.
     *
     * @param phoneNumber The phone number to call.
     */
    @CalledByNative
    @VisibleForTesting
    static void handleMessage(String phoneNumber) {
        ClickToCallUma.recordMessageReceived();

        if (shouldOpenDialer()) {
            openDialer(phoneNumber);
        }

        if (shouldShowNotification()) {
            displayNotification(phoneNumber);
        }
    }
}
