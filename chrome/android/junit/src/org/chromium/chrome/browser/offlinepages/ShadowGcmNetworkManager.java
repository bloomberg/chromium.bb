// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import com.google.android.gms.gcm.GcmNetworkManager;
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

    @Implementation
    public void schedule(Task task) {
        // Capture the string part divisions so we can check them.
        sTask = task;
    }

    public static Task getScheduledTask() {
        return sTask;
    }
}
