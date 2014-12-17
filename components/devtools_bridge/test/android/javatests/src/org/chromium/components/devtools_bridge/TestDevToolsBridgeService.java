// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Intent;
import android.widget.Toast;

import com.google.ipc.invalidation.external.client.contrib.MultiplexingGcmListener;

/**
 * Service for manual testing DevToolsBridgeServer with GCD signaling.
 */
public final class TestDevToolsBridgeService extends DevToolsBridgeServiceBase {
    private static final String SOCKET_NAME = "chrome_shell_devtools_remote";
    public final int NOTIFICATION_ID = 1;
    public final String DISCONNECT_ALL_CLIENTS_ACTION =
            "action.DISCONNECT_ALL_CLIENTS_ACTION";
    public final String UPDATE_GCM_CHANNEL_ID_ACTION =
            "action.UPDATE_GCM_CHANNEL_ID";

    /**
     * Redirects intents to the TestDevToolsBridgeService.
     */
    public static final class Receiver extends ReceiverBase {
        public Receiver() {
            super(TestDevToolsBridgeService.class);
        }
    }

    @Override
    protected void querySocketName(DevToolsBridgeServer.QuerySocketCallback callback) {
        callback.onSuccess(SOCKET_NAME);
    }

    @Override
    protected void onFirstSessionStarted() {
        startForeground(NOTIFICATION_ID, newForegroundNotification());
    }

    @Override
    protected void onLastSessionStopped() {
        stopForeground(true);
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        if (DISCONNECT_ALL_CLIENTS_ACTION.equals(intent.getAction())) {
            server().closeAllSessions();
        } else if (UPDATE_GCM_CHANNEL_ID_ACTION.equals(intent.getAction())) {
            String channelId = MultiplexingGcmListener.initializeGcm(this);
            if (channelId.isEmpty()) {
                Toast.makeText(this, "Not registered", Toast.LENGTH_SHORT).show();
            } else {
                server().updateCloudMessagesId(channelId, startTask());
                Toast.makeText(this, "Updating. See log for results.", Toast.LENGTH_SHORT).show();
            }
        }
    }

    private PendingIntent newPendingIntent(String action) {
        Intent intent = new Intent(this, getClass());
        intent.setAction(action);
        return PendingIntent.getService(this, 0, intent, 0);
    }

    private Notification newForegroundNotification() {
        return new Notification.Builder(this)
                // Mandatory fields
                .setSmallIcon(android.R.drawable.alert_dark_frame)
                .setContentTitle("TestDevToolsBridgeService")
                .setContentText("Remote debugger connected")

                // Optional
                .addAction(android.R.drawable.ic_delete,
                        "Disconnect", newPendingIntent(DISCONNECT_ALL_CLIENTS_ACTION))
                .addAction(android.R.drawable.ic_menu_manage,
                        "Update GCM channel", newPendingIntent(UPDATE_GCM_CHANNEL_ID_ACTION))
                .setOngoing(true)
                .setWhen(System.currentTimeMillis())

                .build();
    }
}
