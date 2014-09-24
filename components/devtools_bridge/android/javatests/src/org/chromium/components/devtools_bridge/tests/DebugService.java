// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.tests;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.Process;
import android.widget.Toast;

import org.chromium.components.devtools_bridge.LocalTunnelBridge;

import java.io.IOException;

/**
 * Service for testing devtools bridge.
 */
public class DebugService extends Service {
    private static final String PACKAGE = "org.chromium.components.devtools_bridge.tests";
    public static final String START_ACTION = PACKAGE + ".START_ACTION";
    public static final String STOP_ACTION = PACKAGE + ".STOP_ACTION";
    private static final int NOTIFICATION_ID = 1;

    private LocalTunnelBridge mBridge;

    private LocalTunnelBridge createBridge() throws IOException {
        String exposingSocketName = "webview_devtools_remote_" + Integer.valueOf(Process.myPid());
        return new LocalTunnelBridge("chrome_shell_devtools_remote", exposingSocketName);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null) return START_NOT_STICKY;

        String action = intent.getAction();
        if (START_ACTION.equals(action)) {
            return start();
        } else if (STOP_ACTION.equals(action)) {
            return stop();
        }
        return START_NOT_STICKY;
    }

    private int start() {
        if (mBridge != null)
            return START_NOT_STICKY;

        try {
            mBridge = createBridge();
            mBridge.start();
        } catch (Exception e) {
            Toast.makeText(this, "Failed to start", Toast.LENGTH_SHORT).show();
            mBridge.dispose();
            mBridge = null;
            return START_NOT_STICKY;
        }

        startForeground(NOTIFICATION_ID, makeForegroundServiceNotification());
        Toast.makeText(this, "Service started", Toast.LENGTH_SHORT).show();
        return START_STICKY;
    }

    private int stop() {
        if (mBridge == null)
            return START_NOT_STICKY;

        mBridge.stop();
        mBridge.dispose();
        mBridge = null;
        stopSelf();
        Toast.makeText(this, "Service stopped", Toast.LENGTH_SHORT).show();
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return new Binder();
    }

    private Notification makeForegroundServiceNotification() {
        Intent showInfoIntent = new Intent(this, DebugActivity.class);
        showInfoIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent showInfoPendingIntent =
                PendingIntent.getActivity(DebugService.this, 0, showInfoIntent, 0);

        Intent stopIntent = new Intent(this, DebugService.class);
        stopIntent.setAction(STOP_ACTION);
        PendingIntent stopPendingIntent =
                    PendingIntent.getService(DebugService.this, 0, stopIntent, 0);

        return new Notification.Builder(this)
                // Mandatory fiends
                .setSmallIcon(android.R.drawable.alert_dark_frame)
                .setContentTitle("DevTools Bridge")
                .setContentText("DevTools socket local test tunnel works")

                // Optional
                .setContentIntent(showInfoPendingIntent)
                .addAction(android.R.drawable.ic_delete,
                        "Stop", stopPendingIntent)
                .setOngoing(true)
                .setWhen(System.currentTimeMillis())
                .build();
    }
}

