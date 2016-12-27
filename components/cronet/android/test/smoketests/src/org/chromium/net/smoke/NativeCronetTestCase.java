// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;

import java.io.File;

/**
 * Test base class for testing native Engine implementation. This class can import classes from the
 * org.chromium.base package.
 */
public class NativeCronetTestCase extends CronetSmokeTestCase {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "cronet_test";
    private static final String LOGFILE_NAME = "cronet-netlog.json";

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        ContextUtils.initApplicationContext(getContext().getApplicationContext());
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        mTestSupport.loadTestNativeLibrary();
    }

    @Override
    protected void tearDown() throws Exception {
        stopAndSaveNetLog();
        super.tearDown();
    }

    @Override
    protected void initCronetEngine() {
        super.initCronetEngine();
        assertNativeEngine(mCronetEngine);
        startNetLog();
    }

    private void startNetLog() {
        if (mCronetEngine != null) {
            mCronetEngine.startNetLogToFile(
                    PathUtils.getDataDirectory() + "/" + LOGFILE_NAME, false);
        }
    }

    private void stopAndSaveNetLog() {
        if (mCronetEngine == null) return;
        mCronetEngine.stopNetLog();
        File netLogFile = new File(PathUtils.getDataDirectory(), LOGFILE_NAME);
        if (!netLogFile.exists()) return;
        mTestSupport.processNetLog(getContext(), netLogFile);
    }
}
