// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.precache;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.Log;

/**
 * Receives a boot signal to reschedule precaching.
 */
public class PrecacheBootReceiver extends BroadcastReceiver {
    private static final String TAG = "Precache";

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.v(TAG, "received boot signal");
        PrecacheController.schedulePeriodicPrecacheTask(context);
    }
}
