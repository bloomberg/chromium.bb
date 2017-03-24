// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;

import org.chromium.base.BuildInfo;
import org.chromium.base.Log;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Builder to be used on Android O until we target O and these APIs are in the support library.
 */
@TargetApi(26 /* Build.VERSION_CODES.O */)
public class NotificationBuilderForO extends NotificationBuilder {
    private static final String TAG = "NotifBuilderForO";

    public NotificationBuilderForO(Context context, String channelId, String channelName,
            String channelGroupId, String channelGroupName) {
        super(context);
        assert BuildInfo.isAtLeastO();
        NotificationManager notificationManager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        /*
        The code in the try-block uses reflection in order to compile as it calls APIs newer than
        our target version of Android. The equivalent code without reflection is as follows:

            notificationManager.createNotificationChannelGroup(
                    new NotificationChannelGroup(channelGroupId, channelGroupName));
            NotificationChannel channel = new NotificationChannel(
                    channelId, channelName, importance);
            channel.setGroup(channelGroupId);
            channel.setShowBadge(false);
            notificationManager.createNotificationChannel(channel);
            notificationBuilder.setChannel(channelId);

        Note this whole class can be removed once we target O, and channels may be set
        inline where notifications are created, using the pattern above.

        In the longer term we may wish to initialize our channels in response to BOOT_COMPLETED or
        ACTION_PACKAGE_REPLACED, as per Android guidelines.
         */
        try {
            // Create channel group
            Class<?> channelGroupClass = Class.forName("android.app.NotificationChannelGroup");
            Constructor<?> channelGroupConstructor =
                    channelGroupClass.getDeclaredConstructor(String.class, CharSequence.class);
            Object channelGroup =
                    channelGroupConstructor.newInstance(channelGroupId, channelGroupName);

            // Register channel group
            Method createNotificationChannelGroupMethod = notificationManager.getClass().getMethod(
                    "createNotificationChannelGroup", channelGroupClass);
            createNotificationChannelGroupMethod.invoke(notificationManager, channelGroup);

            // Create channel
            Class<?> channelClass = Class.forName("android.app.NotificationChannel");
            Constructor<?> channelConstructor = channelClass.getDeclaredConstructor(
                    String.class, CharSequence.class, int.class);
            Object channel = channelConstructor.newInstance(
                    channelId, channelName, getChannelImportance(channelId));

            // Set group on channel
            Method setGroupMethod = channelClass.getMethod("setGroup", String.class);
            setGroupMethod.invoke(channel, channelGroupId);

            // Set channel to not badge on app icon
            Method setShowBadgeMethod = channelClass.getMethod("setShowBadge", boolean.class);
            setShowBadgeMethod.invoke(channel, false);

            // Register channel
            Method createNotificationChannelMethod = notificationManager.getClass().getMethod(
                    "createNotificationChannel", channelClass);
            createNotificationChannelMethod.invoke(notificationManager, channel);

            // Set channel on builder
            Method setChannelMethod =
                    Notification.Builder.class.getMethod("setChannel", String.class);
            setChannelMethod.invoke(mBuilder, channelId);
        } catch (ClassNotFoundException | NoSuchMethodException | IllegalAccessException
                | InstantiationException | InvocationTargetException e) {
            Log.e(TAG, "Error initializing notification builder:", e);
        }
    }

    private static int getChannelImportance(String channelId) {
        return NotificationConstants.CHANNEL_ID_SITES.equals(channelId)
                ? NotificationManager.IMPORTANCE_DEFAULT
                : NotificationManager.IMPORTANCE_LOW;
    }
}
