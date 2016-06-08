// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.runtime_library;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.Bundle;
import android.os.Parcel;
import android.os.RemoteException;
import android.util.Log;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Implements services offered by the WebAPK to Chrome.
 */
public class WebApkServiceImpl extends IWebApkApi.Stub {

    public static final String KEY_SMALL_ICON_ID = "small_icon_id";
    public static final String KEY_HOST_BROWSER_PACKAGE = "host_browser_package";

    private static final String TAG = "WebApkServiceImpl";

    private final Context mContext;

    /**
     * Id of icon to represent WebAPK notifications in status bar.
     */
    private final int mSmallIconId;

    /**
     * Package name of only process allowed to call the service's methods. If a process with a
     * different package name calls the service, the service throws a RemoteException.
     */
    private final String mHostPackage;

    /**
     * Uids of processes which are allowed to call the service.
     */
    private final Set<Integer> mAuthorizedUids = new HashSet<>();

    /**
     * Creates an instance of WebApkServiceImpl.
     * @param context
     * @param bundle Bundle with additional constructor parameters.
     */
    public WebApkServiceImpl(Context context, Bundle bundle) {
        mContext = context;
        mSmallIconId = bundle.getInt(KEY_SMALL_ICON_ID);
        mHostPackage = bundle.getString(KEY_HOST_BROWSER_PACKAGE);
        assert mHostPackage != null;
    }

    boolean checkHasAccess(int uid, PackageManager packageManager) {
        String[] callingPackageNames = packageManager.getPackagesForUid(uid);
        Log.d(TAG, "Verifying bind request from: " + Arrays.toString(callingPackageNames) + " "
                        + mHostPackage);
        if (callingPackageNames == null) {
            return false;
        }

        for (String packageName : callingPackageNames) {
            if (packageName.equals(mHostPackage)) {
                return true;
            }
        }

        Log.e(TAG, "Unauthorized request from uid: " + uid);
        return false;
    }

    @Override
    public boolean onTransact(int arg0, Parcel arg1, Parcel arg2, int arg3) throws RemoteException {
        int callingUid = Binder.getCallingUid();
        if (!mAuthorizedUids.contains(callingUid)) {
            if (checkHasAccess(callingUid, mContext.getPackageManager())) {
                mAuthorizedUids.add(callingUid);
            } else {
                throw new RemoteException("Unauthorized caller " + callingUid
                        + " does not match expected host=" + mHostPackage);
            }
        }
        return super.onTransact(arg0, arg1, arg2, arg3);
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
