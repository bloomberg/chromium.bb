// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import org.chromium.base.PathUtils;
import org.chromium.base.ResourceExtractor;
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

    private static final String[] MANDATORY_PAK_FILES = new String[] {"cast_shell.pak"};
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "cast_shell";

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
