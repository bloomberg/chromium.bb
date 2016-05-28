// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.runtime_library;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Bundle;

/**
 * Implements services offered by the WebAPK to Chrome.
 */
public class WebApkServiceImpl extends IWebApkApi.Stub {

    public static final String KEY_SMALL_ICON_ID = "small_icon_id";

    private static final String TAG = "WebApkServiceImpl";

    private final Context mContext;

    /**
     * Id of icon to represent WebAPK notifications in status bar.
     */
    private final int mSmallIconId;

    /**
     * Creates an instance of WebApkServiceImpl.
     * @param context
     * @param bundle Bundle with additional constructor parameters.
     */
    public WebApkServiceImpl(Context context, Bundle bundle) {
        mContext = context;
        mSmallIconId = bundle.getInt(KEY_SMALL_ICON_ID);
    }

    @Override
    public int getSmallIconId() {
        return mSmallIconId;
    }

    @Override
    public void notifyNotification(String platformTag, int platformID, Notification notification) {
        getNotificationManager().notify(platformTag, platformID, notification);
    }

    @Override
    public void cancelNotification(String platformTag, int platformID) {
        getNotificationManager().cancel(platformTag, platformID);
    }

    private NotificationManager getNotificationManager() {
        return (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
    }
}
