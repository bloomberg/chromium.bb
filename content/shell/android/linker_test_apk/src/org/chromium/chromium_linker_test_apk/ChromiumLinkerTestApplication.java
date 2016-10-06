// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromium_linker_test_apk;

import android.content.Context;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;

/**
 * Application for testing the Chromium Linker
 */
public class ChromiumLinkerTestApplication extends BaseChromiumApplication {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chromium_linker_test";

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        ContextUtils.initApplicationContext(this);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
    }
}
