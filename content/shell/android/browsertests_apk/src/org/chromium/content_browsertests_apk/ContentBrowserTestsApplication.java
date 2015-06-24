// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_browsertests_apk;

import android.content.Context;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.PathUtils;

/**
 * A basic content browser tests {@link android.app.Application}.
 */
public class ContentBrowserTestsApplication extends BaseChromiumApplication {

    static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "content_shell";

    @Override
    public void onCreate() {
        super.onCreate();
        initializeApplicationParameters(this);
    }

    public static void initializeApplicationParameters(Context context) {
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX, context);
    }

}
