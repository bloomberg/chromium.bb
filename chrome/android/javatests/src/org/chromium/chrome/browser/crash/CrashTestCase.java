// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.test.InstrumentationTestCase;
import android.util.Log;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.Arrays;
import java.util.List;

/**
 * Base case for Crash upload related tests.
 */
public class CrashTestCase extends InstrumentationTestCase {
    private static final String TAG = "CrashTestCase";

    protected File mCrashDir;
    protected File mCacheDir;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mCacheDir = getInstrumentation().getTargetContext().getCacheDir();
        mCrashDir = new File(
                mCacheDir,
                CrashFileManager.CRASH_DUMP_DIR);
        if (!mCrashDir.isDirectory() && !mCrashDir.mkdir()) {
            throw new Exception("Unable to create directory: " + mCrashDir.getAbsolutePath());
        }
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        List<File> crashFiles = Arrays.asList(mCrashDir.listFiles());
        for (File crashFile : crashFiles) {
            if (!crashFile.delete()) {
                Log.e(TAG, "Unable to delete: " + crashFile.getAbsolutePath());
            }
        }
        if (!mCrashDir.delete()) {
            Log.e(TAG, "Unable to delete: " + mCrashDir.getAbsolutePath());
        }
    }

    protected static void setUpMinidumpFile(File file, String boundary) throws IOException {
        PrintWriter minidumpWriter = null;
        try {
            minidumpWriter = new PrintWriter(new FileWriter(file));
            minidumpWriter.println("--" + boundary);
            minidumpWriter.println("Content-Disposition: form-data; name=\"prod\"");
            minidumpWriter.println();
            minidumpWriter.println("Chrome_Android");
            minidumpWriter.println("--" + boundary);
            minidumpWriter.println("Content-Disposition: form-data; name=\"ver\"");
            minidumpWriter.println();
            minidumpWriter.println("1");
            minidumpWriter.println(boundary + "--");
            minidumpWriter.flush();
            minidumpWriter.close();
        } finally {
            if (minidumpWriter != null) {
                minidumpWriter.close();
            }
        }
    }
}
