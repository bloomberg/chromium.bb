// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo_shell_apk;

import android.util.Log;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.PathUtils;

public class MojoShellApplication extends BaseChromiumApplication {
    private static final String TAG = "MojoShellApplication";
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "mojo_shell";

    @Override
    public void onCreate() {
        super.onCreate();
        initializeApplicationParameters();
    }

    public static void initializeApplicationParameters() {
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        Log.i(TAG, "MojoShellApplication.initializeApplicationParameters() success.");
    }

}
