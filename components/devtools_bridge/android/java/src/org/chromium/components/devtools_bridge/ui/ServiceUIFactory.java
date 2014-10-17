// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.ui;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;

/**
 * UI factory for DevToolsBridgeServer.
 */
public abstract class ServiceUIFactory {
    public Notification newForegroundNotification(Service host, String disconnectAction) {
        // TODO(serya): move strings to a string grid.

        Intent disconnectIntent = new Intent(host, host.getClass());
        disconnectIntent.setAction(disconnectAction);
        PendingIntent disconnectPendingIntent =
                PendingIntent.getService(host, 0, disconnectIntent, 0);

        Notification.Builder builder = new Notification.Builder(host)
                // Mandatory fields
                .setSmallIcon(notificationSmallIcon())
                .setContentTitle(productName())
                .setContentText("Remote debugger connected")

                // Optional
                .addAction(android.R.drawable.ic_delete,
                        "Disconnect", disconnectPendingIntent)
                .setOngoing(true)
                .setWhen(System.currentTimeMillis());

        setupContentIntent(builder);
        return builder.build();
    }

    protected abstract String productName();
    protected abstract int notificationSmallIcon();
    protected void setupContentIntent(Notification.Builder builder) {}
}
