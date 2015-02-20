// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromium_linker_test_apk;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.PathUtils;
import org.chromium.base.ResourceExtractor;

/**
 * Application for testing the Chromium Linker
 */
public class ChromiumLinkerTestApplication extends BaseChromiumApplication {

    /**
     * icudtl.dat provides ICU (i18n library) with all the data for its
     * operation. We use to link it statically to our binary, but not any more
     * so that we have to install it along with other mandatory pak files.
     * See src/third_party/icu/README.chromium.
     *
     *  V8's initial snapshot used to be statically linked to the binary, but
     *  now it's loaded from external files. Therefore we need to install such
     *  snapshots (natives_blob.bin and snapshot.bin) along with pak files.
     */
    private static final String[] MANDATORY_PAK_FILES = new String[] {
        "content_shell.pak",
        "icudtl.dat",
        "natives_blob.bin",
        "snapshot_blob.bin"
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
