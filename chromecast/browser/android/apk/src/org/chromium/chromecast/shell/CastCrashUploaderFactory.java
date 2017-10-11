// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;

/**
 * Factory class that creates CastCrashUploader using a ScheduledExecutorService singleton to
 * schedule all tasks on one thread.
 */
public final class CastCrashUploaderFactory {
    private static ScheduledExecutorService sExecutorService = null;

    public static CastCrashUploader createCastCrashUploader(
            String crashDumpPath, String uuid, boolean uploadCrashToStaging) {
        if (sExecutorService == null) {
            sExecutorService = Executors.newScheduledThreadPool(1);
        }
        return new CastCrashUploader(sExecutorService, crashDumpPath, uuid, uploadCrashToStaging);
    }
}
