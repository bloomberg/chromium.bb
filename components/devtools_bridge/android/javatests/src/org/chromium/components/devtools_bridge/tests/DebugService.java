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

import org.chromium.components.devtools_bridge.DevToolsBridgeServerSandbox;
import org.chromium.components.devtools_bridge.GCDClientSessionTestingHost;
import org.chromium.components.devtools_bridge.LocalSessionBridge;

import java.io.IOException;

/**
 * Service for testing devtools bridge.
 */
public class DebugService extends Service {
    public static final String START_SESSION_BRIDGE_ACTION = "action.START_SESSION_BRIDGE";
    public static final String START_SERVER_ACTION = "action.START_SERVER";
    public static final String START_GCD_CLIENT_ACTION = "action.START_GCD_CLIENT";
    public static final String STOP_ACTION = "action.STOP";

    public static final String EXTRA_OAUTH_TOKEN = "extra.OAUTH_TOKEN";
    public static final String EXTRA_REMOTE_INSTANCE_ID = "extra.REMOTE_INSTANCE_ID";

    private static final int NOTIFICATION_ID = 1;

    private Controller mRunningController;

    private interface Controller {
        void create() throws IOException;
        void start() throws Exception;
        void stop();
        void dispose();
    }

    private String replicatingSocketName() {
        return "chrome_shell_devtools_remote";
    }

    private String exposingSocketName() {
        return "webview_devtools_remote_" + Integer.valueOf(Process.myPid());
    }

    private class LocalSessionBridgeController implements Controller {
        private LocalSessionBridge mBridge;

        @Override
        public void create() throws IOException {
            mBridge = new LocalSessionBridge(replicatingSocketName(), exposingSocketName());
        }

        @Override
        public void start() {
            mBridge.start();
        }

        @Override
        public void stop() {
            mBridge.stop();
        }

        @Override
        public void dispose() {
            mBridge.dispose();
        }

        @Override
        public String toString() {
            return "LocalSessionBridge";
        }
    }

    private class DevToolsBridgeServerSandboxController implements Controller {
        private DevToolsBridgeServerSandbox mSandbox;

        @Override
        public void create() {
            mSandbox = new DevToolsBridgeServerSandbox();
        }

        @Override
        public void start() throws Exception {
            mSandbox.start(DebugService.this);
        }

        @Override
        public void stop() {
            mSandbox.stop();
        }

        @Override
        public void dispose() {}

        @Override
        public String toString() {
            return "DevToolsBridgeServerSandbox";
        }
    }

    private class GCDClientController implements Controller {
        private final String mOAuthToken;
        private final String mRemoteInstanceId;

        private GCDClientSessionTestingHost mHost;

        public GCDClientController(String oAuthToken, String remoteInstanceId) {
            mOAuthToken = oAuthToken;
            mRemoteInstanceId = remoteInstanceId;
        }

        @Override
        public void create() {
        }

        @Override
        public void start() throws Exception {
            final GCDClientSessionTestingHost host = new GCDClientSessionTestingHost(
                    mOAuthToken, exposingSocketName(), mRemoteInstanceId);
            host.start(new Runnable() {
                        @Override
                        public void run() {
                            String text;
                            if (!host.isStarted()) {
                                text = "Session not started. See log for details";
                            } else {
                                text = "Session started";
                            }
                            Toast.makeText(DebugService.this, text, Toast.LENGTH_SHORT).show();
                        }
                    });
            mHost = host;
        }

        @Override
        public void stop() {
        }

        @Override
        public void dispose() {
            mHost.dispose();
        }

        @Override
        public String toString() {
            return "GCDClientSessionTestingHost";
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null) return START_NOT_STICKY;

        String action = intent.getAction();
        if (START_SESSION_BRIDGE_ACTION.equals(action)) {
            return start(new LocalSessionBridgeController());
        } else if (START_SERVER_ACTION.equals(action)) {
            return start(new DevToolsBridgeServerSandboxController());
        } else if (START_GCD_CLIENT_ACTION.equals(action)) {
            String oAuthToken = intent.getStringExtra(EXTRA_OAUTH_TOKEN);
            String remoteInstanceId = intent.getStringExtra(EXTRA_REMOTE_INSTANCE_ID);
            return start(new GCDClientController(oAuthToken, remoteInstanceId));
        } else if (STOP_ACTION.equals(action)) {
            return stop();
        }
        return START_NOT_STICKY;
    }

    private int start(Controller controller) {
        if (mRunningController != null) {
            Toast.makeText(this, "Already started", Toast.LENGTH_SHORT).show();
            return START_NOT_STICKY;
        }

        try {
            controller.create();
            controller.start();
        } catch (Exception e) {
            e.printStackTrace();
            Toast.makeText(this, "Failed to start", Toast.LENGTH_SHORT).show();
            controller.dispose();
            return START_NOT_STICKY;
        }
        mRunningController = controller;

        startForeground(NOTIFICATION_ID, makeForegroundServiceNotification());
        Toast.makeText(this, controller.toString() + " started", Toast.LENGTH_SHORT).show();
        return START_STICKY;
    }

    private int stop() {
        if (mRunningController == null)
            return START_NOT_STICKY;

        String name = mRunningController.toString();
        mRunningController.stop();
        mRunningController.dispose();
        mRunningController = null;
        stopSelf();
        Toast.makeText(this, name + " stopped", Toast.LENGTH_SHORT).show();
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

