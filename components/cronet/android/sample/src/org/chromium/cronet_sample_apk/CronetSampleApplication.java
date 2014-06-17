// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import android.app.Application;
import android.util.Log;

import org.chromium.base.PathUtils;

/**
 * Application for managing the Cronet Sample.
 */
public class CronetSampleApplication extends Application {
    private static final String TAG = "CronetSampleApplication";

    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "cronet_sample";

    @Override
    public void onCreate() {
        super.onCreate();
        initializeApplicationParameters();
    }

    public static void initializeApplicationParameters() {
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        Log.i(TAG, "CronetSampleApplication.initializeApplicationParameters()" +
                   " success.");
    }
}
