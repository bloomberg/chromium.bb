// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.provider.Browser;
import android.support.v4.app.NotificationCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;

/**
 * Provides functionality needed for content suggestion notifications.
 *
 * Exposes helper functions to native C++ code.
 */
public class ContentSuggestionsNotificationHelper {
    private static final String NOTIFICATION_TAG = "ContentSuggestionsNotification";
    private static final int NOTIFICATION_ID = 0;

    private ContentSuggestionsNotificationHelper() {} // Prevent instantiation

    @CalledByNative
    private static void openUrl(String url) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));
        intent.setClass(context, ChromeLauncherActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentHandler.addTrustedIntentExtras(intent);
        context.startActivity(intent);
    }

    @CalledByNative
    private static void showNotification(String url, String title, String text, Bitmap image) {
        Context context = ContextUtils.getApplicationContext();
        NotificationManager manager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        Intent intent = new Intent()
                                .setAction(Intent.ACTION_VIEW)
                                .setData(Uri.parse(url))
                                .setClass(context, ChromeLauncherActivity.class)
                                .putExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true)
                                .putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        NotificationCompat.Builder builder =
                new NotificationCompat.Builder(context)
                        .setAutoCancel(true)
                        .setContentIntent(PendingIntent.getActivity(context, 0, intent, 0))
                        .setContentTitle(title)
                        .setContentText(text)
                        .setGroup(NOTIFICATION_TAG)
                        .setLargeIcon(image)
                        .setSmallIcon(R.drawable.ic_chrome);
        manager.notify(NOTIFICATION_TAG, NOTIFICATION_ID, builder.build());
    }

    @CalledByNative
    private static void hideNotification() {
        Context context = ContextUtils.getApplicationContext();
        NotificationManager manager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        manager.cancel(NOTIFICATION_TAG, NOTIFICATION_ID);
    }
}
