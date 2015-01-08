// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.battery;

import org.chromium.mojo.system.MojoException;
import org.chromium.mojom.device.BatteryMonitor;
import org.chromium.mojom.device.BatteryStatus;

import java.util.ArrayList;
import java.util.List;

/**
 * Android implementation of the battery monitor service defined in
 * device/battery/battery_monitor.mojom.
 */
public class BatteryMonitorImpl implements BatteryMonitor {
    // Factory that created this instance and notifies it about battery status changes.
    private final BatteryMonitorFactory mFactory;
    private final List<QueryNextStatusResponse> mCallbacks;
    private BatteryStatus mStatus;
    private boolean mHasStatusToReport;
    private boolean mSubscribed;

    public BatteryMonitorImpl(BatteryMonitorFactory batteryMonitorFactory) {
        mFactory = batteryMonitorFactory;
        mCallbacks = new ArrayList<QueryNextStatusResponse>();
        mHasStatusToReport = false;
        mSubscribed = true;
    }

    private void unsubscribe() {
        if (mSubscribed) {
            mFactory.unsubscribe(this);
            mSubscribed = false;
        }
    }

    @Override
    public void close() {
        unsubscribe();
    }

    @Override
    public void onConnectionError(MojoException e) {
        unsubscribe();
    }

    @Override
    public void queryNextStatus(QueryNextStatusResponse callback) {
        mCallbacks.add(callback);

        if (mHasStatusToReport) {
            reportStatus();
        }
    }

    void didChange(BatteryStatus batteryStatus) {
        mStatus = batteryStatus;
        mHasStatusToReport = true;

        if (!mCallbacks.isEmpty()) {
            reportStatus();
        }
    }

    void reportStatus() {
        for (QueryNextStatusResponse callback : mCallbacks) {
            callback.call(mStatus);
        }
        mCallbacks.clear();
        mHasStatusToReport = false;
    }
}
