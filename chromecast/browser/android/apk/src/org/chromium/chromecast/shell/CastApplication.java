// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.content.app.ContentApplication;

/**
 * Entry point for the Android cast shell application.  Handles initialization of information that
 * needs to be shared across the main activity and the child services created.
 *
 * Note that this gets run for each process, including sandboxed child render processes. Child
 * processes don't need most of the full "setup" performed in CastBrowserHelper.java, but they do
 * require a few basic pieces (found here).
 */
public class CastApplication extends ContentApplication {
    private static final String TAG = "CastApplication";

    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "cast_shell";
    private static final String COMMAND_LINE_FILE = "castshell-command-line";

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        ContextUtils.initApplicationContext(this);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        ApplicationStatus.initialize(this);
    }

    public void initCommandLine() {
        CommandLineInitUtil.initCommandLine(this, COMMAND_LINE_FILE);
    }
}
