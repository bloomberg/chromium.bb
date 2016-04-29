// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Provides Java scheduling support from native offlining code as
 * well as JNI interface to tell native code to start processing
 * queued requests.
 */
@JNINamespace("offline_pages::android")
public class BackgroundSchedulerBridge {

    /**
     * Callback used to determine when request processing is done.
     */
    public interface ProcessingDoneCallback {
        @CalledByNative("ProcessingDoneCallback")
        void onProcessingDone(boolean result);
    }

    // Starts processing of one or more queued background requests.
    // Returns whether processing was started and that caller should
    // expect a callback (once processing has completed or terminated).
    // If processing was already active or not able to process for
    // some other reason, returns false and this calling instance will
    // not receive a callback.
    // TODO(dougarnett): consider adding policy check api to let caller
    //     separately determine if not allowed by policy.
    public static boolean startProcessing(
            Profile profile, ProcessingDoneCallback callback) {
        return nativeStartProcessing(profile, callback);
    }

    @CalledByNative
    private static void schedule() {
        // TODO(dougarnett): call GcmNetworkManager to schedule for
        //     OfflinePageUtils.TASK_TAG.
    }

    @CalledByNative
    private static void unschedule() {
        // TODO(dougarnett): call GcmNetworkManager to unschedule for
        //     OfflinePageUtils.TASK_TAG.
    }

    private static native boolean nativeStartProcessing(
            Profile profile, ProcessingDoneCallback callback);
}

