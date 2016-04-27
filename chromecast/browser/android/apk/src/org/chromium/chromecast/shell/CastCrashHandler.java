// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chromecast.base.ChromecastConfigAndroid;

/**
 * JNI wrapper class for accessing CastCrashHandler.
 */
@JNINamespace("chromecast")
public final class CastCrashHandler {
    private static final String TAG = "cr_CastCrashHandler";

    @CalledByNative
    public static void initializeUploader(Context context, String crashDumpPath,
            boolean uploadCrashToStaging) {
        CastCrashUploader uploader = new CastCrashUploader(crashDumpPath, uploadCrashToStaging);
        if (ChromecastConfigAndroid.canSendUsageStats(context)) {
            uploader.startPeriodicUpload();
        } else {
            Log.d(TAG, "Removing crash dumps instead of uploading");
            uploader.removeCrashDumps();
        }
    }
}
