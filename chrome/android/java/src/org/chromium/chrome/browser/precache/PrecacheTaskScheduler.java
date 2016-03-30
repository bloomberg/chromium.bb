// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.precache;

import android.content.Context;

import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.Task;

import org.chromium.chrome.browser.ChromeBackgroundService;

class PrecacheTaskScheduler {
    void scheduleTask(Context context, Task task) {
        GcmNetworkManager.getInstance(context).schedule(task);
    }

    void cancelTask(Context context, String tag) {
        GcmNetworkManager.getInstance(context).cancelTask(tag, ChromeBackgroundService.class);
    }
}
