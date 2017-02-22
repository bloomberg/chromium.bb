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
    private Task mTask;
    private Task mCanceledTask;

    @Implementation
    public void schedule(Task task) {
        // Capture the string part divisions so we can check them.
        mTask = task;
    }

    @Implementation
    public void cancelTask(String tag, Class<? extends GcmTaskService> gcmTaskService) {
        if (mTask != null && mTask.getTag().equals(tag)
                && mTask.getServiceName().equals(gcmTaskService.getName())) {
            mCanceledTask = mTask;
            mTask = null;
        }
    }

    public Task getScheduledTask() {
        return mTask;
    }

    public Task getCanceledTask() {
        return mCanceledTask;
    }

    public void clear() {
        mTask = null;
        mCanceledTask = null;
    }
}
