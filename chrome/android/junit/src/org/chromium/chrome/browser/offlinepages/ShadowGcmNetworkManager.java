// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.GcmTaskService;
import com.google.android.gms.gcm.Task;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;


/**
 * Custom shadow for the OS's GcmNetworkManager.  We use this to hook the call to GcmNetworkManager
 * to make sure it was invoked as we expect.
 */
@Implements(GcmNetworkManager.class)
public class ShadowGcmNetworkManager {
    private static Task sTask;
    private static Task sCanceledTask;

    @Implementation
    public static void schedule(Task task) {
        // Capture the string part divisions so we can check them.
        sTask = task;
    }

    @Implementation
    public static void cancelTask(String tag, Class<? extends GcmTaskService> gcmTaskService) {
        if (sTask != null && sTask.getTag().equals(tag)
                && sTask.getServiceName().equals(gcmTaskService.getName())) {
            sCanceledTask = sTask;
            sTask = null;
        }
    }

    public static Task getScheduledTask() {
        return sTask;
    }

    public static Task getCanceledTask() {
        return sCanceledTask;
    }

    public static void clear() {
        sTask = null;
        sCanceledTask = null;
    }
}
