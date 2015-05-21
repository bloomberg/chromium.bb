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
        PrintWriter miniDumpWriter = null;
        try {
            miniDumpWriter = new PrintWriter(new FileWriter(file));
            miniDumpWriter.println("--" + boundary);
            miniDumpWriter.println("Content-Disposition: form-data; name=\"prod\"");
            miniDumpWriter.println();
            miniDumpWriter.println("Chrome_Android");
            miniDumpWriter.println("--" + boundary);
            miniDumpWriter.println("Content-Disposition: form-data; name=\"ver\"");
            miniDumpWriter.println();
            miniDumpWriter.println("1");
            miniDumpWriter.println(boundary + "--");
            miniDumpWriter.flush();
            miniDumpWriter.close();
        } finally {
            if (miniDumpWriter != null) {
                miniDumpWriter.close();
            }
        }
    }
}
