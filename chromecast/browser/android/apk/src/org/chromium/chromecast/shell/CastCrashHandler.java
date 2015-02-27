// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * JNI wrapper class for accessing CastCrashHandler.
 */
@JNINamespace("chromecast")
public final class CastCrashHandler {
    private static CastCrashUploader sCrashUploader;

    @CalledByNative
    public static void initializeUploader(String crashDumpPath, boolean uploadCrashToStaging) {
        if (sCrashUploader == null) {
            sCrashUploader = new CastCrashUploader(crashDumpPath, uploadCrashToStaging);
            sCrashUploader.startPeriodicUpload();
        }
    }

    @CalledByNative
    public static void removeCrashDumps() {
        sCrashUploader.removeCrashDumps();
    }

    @CalledByNative
    public static void uploadCrashDumps(String logFilePath) {
        sCrashUploader.uploadCrashDumps(logFilePath);
    }
}
