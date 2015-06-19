// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromium_linker_test_apk;

import android.content.Context;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.PathUtils;
import org.chromium.base.ResourceExtractor;

/**
 * Application for testing the Chromium Linker
 */
public class ChromiumLinkerTestApplication extends BaseChromiumApplication {

    /**
     *  V8's initial snapshot used to be statically linked to the binary, but
     *  now it's loaded from external files. Therefore we need to install such
     *  snapshots (natives_blob.bin and snapshot.bin) along with pak files.
     */
    private static final String[] MANDATORY_PAK_FILES = new String[] {
        "content_shell.pak",
        "natives_blob.bin",
        "snapshot_blob.bin"
    };
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chromium_linker_test";

    @Override
    public void onCreate() {
        super.onCreate();
        initializeApplicationParameters(this);
    }

    public static void initializeApplicationParameters(Context context) {
        ResourceExtractor.setMandatoryPaksToExtract(MANDATORY_PAK_FILES);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX, context);
    }

}
