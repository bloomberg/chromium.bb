// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import org.chromium.base.CommandLine;
import org.chromium.base.PathUtils;
import org.chromium.base.ResourceExtractor;

import org.chromium.content.app.ContentApplication;

/**
 * Entry point for the content shell application.  Handles initialization of information that needs
 * to be shared across the main activity and the child services created.
 */
public class ContentShellApplication extends ContentApplication {

    public static final String COMMAND_LINE_FILE = "/data/local/tmp/content-shell-command-line";
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
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "content_shell";

    @Override
    public void onCreate() {
        super.onCreate();
    }

    @Override
    protected void initializeLibraryDependencies() {
        ResourceExtractor.setMandatoryPaksToExtract(MANDATORY_PAK_FILES);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
    }

    @Override
    public void initCommandLine() {
        if (!CommandLine.isInitialized()) {
            CommandLine.initFromFile(COMMAND_LINE_FILE);
        }
    }

}
