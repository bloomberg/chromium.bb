// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromium_linker_test_apk;

import android.app.Application;

import org.chromium.base.PathUtils;
import org.chromium.base.ResourceExtractor;

/**
 * Application for testing the Chromium Linker
 */
public class ChromiumLinkerTestApplication extends Application {

    /**
     * icudtl.dat provides ICU (i18n library) with all the data for its
     * operation. We use to link it statically to our binary, but not any more
     * so that we have to install it along with other mandatory pak files.
     * See src/third_party/icu/README.chromium.
     */
    private static final String[] MANDATORY_PAK_FILES = new String[] {
        "content_shell.pak",
        "icudtl.dat"
    };
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chromium_linker_test";

    @Override
    public void onCreate() {
        super.onCreate();
        initializeApplicationParameters();
    }

    public static void initializeApplicationParameters() {
        ResourceExtractor.setMandatoryPaksToExtract(MANDATORY_PAK_FILES);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
    }

}
