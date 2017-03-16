// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.crash;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.PersistableBundle;

import org.chromium.android_webview.command_line.CommandLineUtil;
import org.chromium.base.ContextUtils;
import org.chromium.components.minidump_uploader.MinidumpUploadJobService;
import org.chromium.components.minidump_uploader.MinidumpUploader;
import org.chromium.components.minidump_uploader.MinidumpUploaderImpl;

/**
 * Class that interacts with the Android JobScheduler to upload Minidumps at appropriate times.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
// OBS: This class needs to be public to be started from android.app.ActivityThread.
public class AwMinidumpUploadJobService extends MinidumpUploadJobService {
    @Override
    public void onCreate() {
        super.onCreate();
        // This overwrites the command line set by ChromeApplication.onCreate() to use a
        // WebView-specific command line file. This is okay since this Service is not running in the
        // same process as the main Chrome process.
        CommandLineUtil.initCommandLine();
    }

    @Override
    protected MinidumpUploader createMinidumpUploader(PersistableBundle unusedExtras) {
        // Ensure we can use ContextUtils later on (from the minidump_uploader component).
        ContextUtils.initApplicationContext(this.getApplicationContext());

        return new MinidumpUploaderImpl(new AwMinidumpUploaderDelegate(this));
    }
}
