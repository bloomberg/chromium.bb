// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.battery;

import org.chromium.mojo.system.MojoException;
import org.chromium.mojom.device.BatteryMonitor;
import org.chromium.mojom.device.BatteryStatus;
import org.chromium.mojom.device.BatteryStatusObserver;

/**
 * Android implementation of the battery monitor service defined in
 * device/battery/battery_monitor.mojom.
 */
public class BatteryMonitorImpl implements BatteryMonitor {
    // Factory that created this instance and notifies it about battery status changes.
    private final BatteryMonitorFactory mFactory;
    private boolean mSubscribed;

    private BatteryStatusObserver mClient;

    public BatteryMonitorImpl(BatteryMonitorFactory batteryMonitorFactory) {
        mFactory = batteryMonitorFactory;
        mSubscribed = true;
    }

    private void unsubscribe() {
        if (mSubscribed) {
            mFactory.unsubscribe(this);
            mSubscribed = false;
        }
    }

    @Override
    public void setClient(BatteryStatusObserver client) {
        mClient = client;
    }

    @Override
    public void close() {
        unsubscribe();
    }

    @Override
    public void onConnectionError(MojoException e) {
        unsubscribe();
    }

    /**
     * Notifies the client passing the given battery status information.
     */
    void didChange(BatteryStatus batteryStatus) {
        if (mClient != null) {
            mClient.didChange(batteryStatus);
        }
    };
}
