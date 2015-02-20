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

    @CalledByNative
    public static void removeCrashDumpsSync(String crashDumpPath, boolean uploadCrashToStaging) {
        new CastCrashUploader(crashDumpPath, uploadCrashToStaging).removeCrashDumpsSync();
    }

    @CalledByNative
    public static void uploadCurrentProcessDumpSync(String crashDumpPath, String logFilePath,
            boolean uploadCrashToStaging) {
        new CastCrashUploader(crashDumpPath, uploadCrashToStaging)
                .uploadCurrentProcessDumpSync(logFilePath);
    }

    @CalledByNative
    public static void uploadCrashDumpsAsync(String crashDumpPath, boolean uploadCrashToStaging) {
        new CastCrashUploader(crashDumpPath, uploadCrashToStaging).uploadRecentCrashesAsync();
    }
}
