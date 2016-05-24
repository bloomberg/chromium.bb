// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.precache;

import android.content.Context;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.Task;

import org.chromium.chrome.browser.ChromeBackgroundService;

class PrecacheTaskScheduler {
    boolean scheduleTask(Context context, Task task) {
        if (GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(context)
                != ConnectionResult.SUCCESS) {
            return false;
        }
        try {
            GcmNetworkManager.getInstance(context).schedule(task);
        } catch (IllegalArgumentException e) {
            return false;
        }
        return true;
    }

    boolean cancelTask(Context context, String tag) {
        if (GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(context)
                != ConnectionResult.SUCCESS) {
            return false;
        }
        try {
            GcmNetworkManager.getInstance(context).cancelTask(tag, ChromeBackgroundService.class);
        } catch (IllegalArgumentException e) {
            return false;
        }
        return true;
    }
}
