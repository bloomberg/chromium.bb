// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_test_apk;

import android.app.Application;
import android.util.Log;

import org.chromium.base.PathUtils;

/**
 * Application for managing the Cronet Test.
 */
public class CronetTestApplication extends Application {
    private static final String TAG = "CronetTestApplication";

    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "cronet_test";

    @Override
    public void onCreate() {
        super.onCreate();
        initializeApplicationParameters();
    }

    public static void initializeApplicationParameters() {
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        Log.i(TAG, "CronetTestApplication.initializeApplicationParameters()" +
                   " success.");
    }
}
