// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.annotation.TargetApi;
import android.app.Service;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.IBinder;

import org.chromium.base.ContextUtils;

/**
 * Broadcasts Physical Web URLs via Bluetooth Low Energy (BLE).
 *
 * This background service will start when the user activates the Physical Web Sharing item in the
 * sharing chooser intent. If the URL received from the triggering Tab is validated
 * by the Physical Web Service then it will encode the URL into an Eddystone URL and
 * broadcast the URL through BLE.
 *
 * To notify and provide the user with the ability to terminate broadcasting
 * a persistent notification with a cancel button will be created. This cancel button will stop the
 * service and will be the only way for the user to stop the service.
 *
 * If the user terminates Chrome or the OS shuts down this service mid-broadcasting, when the
 * service get's restarted it will restart broadcasting as well by gathering the URL from
 * shared preferences. This will create a seemless experience to the user by behaving as
 * though broadcasting is not connected to the Chrome application.
 *
 * bluetooth.le.AdvertiseCallback and bluetooth.BluetoothAdapter require API level 21.
 * This will only be run on M and above.
 **/

@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class PhysicalWebBroadcastService extends Service {
    public static final String DISPLAY_URL_KEY = "display_url";

    public static final String PREVIOUS_BROADCAST_URL_KEY = "previousUrl";

    private boolean mStartedByRestart;

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String displayUrl = fetchDisplayUrl(intent);

        // This should never happen.
        if (displayUrl == null) {
            stopSelf();
            return START_STICKY;
        }

        // TODO(iankc): implement parsing, broadcasting, and notifications.
        stopSelf();
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private String fetchDisplayUrl(Intent intent) {
        mStartedByRestart = intent == null;
        if (mStartedByRestart) {
            SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
            return sharedPrefs.getString(PREVIOUS_BROADCAST_URL_KEY, null);
        }

        String displayUrl = intent.getStringExtra(DISPLAY_URL_KEY);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putString(PREVIOUS_BROADCAST_URL_KEY, displayUrl)
                .commit();
        return displayUrl;
    }
}
