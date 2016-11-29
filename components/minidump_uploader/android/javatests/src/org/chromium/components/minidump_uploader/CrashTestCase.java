// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import android.test.InstrumentationTestCase;

import org.chromium.base.Log;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;

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
        mCacheDir = getExistingCacheDir();
        mCrashDir = new File(
                mCacheDir,
                CrashFileManager.CRASH_DUMP_DIR);
        if (!mCrashDir.isDirectory() && !mCrashDir.mkdir()) {
            throw new Exception("Unable to create directory: " + mCrashDir.getAbsolutePath());
        }
    }

    /**
     * Returns the cache directory where we should store minidumps.
     * Can be overriden by sub-classes to allow for use with different cache directories.
     */
    protected File getExistingCacheDir() {
        return getInstrumentation().getTargetContext().getCacheDir();
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        File[] crashFiles = mCrashDir.listFiles();
        if (crashFiles == null) {
            return;
        }

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
        setUpMinidumpFile(file, boundary, null);
    }

    protected static void setUpMinidumpFile(File file, String boundary, String processType)
            throws IOException {
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
            if (processType != null) {
                minidumpWriter.println("Content-Disposition: form-data; name=\"ptype\"");
                minidumpWriter.println();
                minidumpWriter.println(processType);
            }
            minidumpWriter.println(boundary + "--");
            minidumpWriter.flush();
        } finally {
            if (minidumpWriter != null) {
                minidumpWriter.close();
            }
        }
    }

    /**
     * A utility instantiation of CrashReportingPermissionManager providing a compact way of
     * overriding different permission settings.
     */
    public static class MockCrashReportingPermissionManager
            implements CrashReportingPermissionManager {
        protected boolean mIsInSample;
        protected boolean mIsPermitted;
        protected boolean mIsUserPermitted;
        protected boolean mIsCommandLineDisabled;
        protected boolean mIsNetworkAvailable;
        protected boolean mIsEnabledForTests;

        MockCrashReportingPermissionManager() {}

        @Override
        public boolean isClientInMetricsSample() {
            return mIsInSample;
        }

        @Override
        public boolean isNetworkAvailableForCrashUploads() {
            return mIsNetworkAvailable;
        }

        @Override
        public boolean isMetricsUploadPermitted() {
            return mIsPermitted;
        }

        @Override
        public boolean isUsageAndCrashReportingPermittedByUser() {
            return mIsUserPermitted;
        }

        @Override
        public boolean isCrashUploadDisabledByCommandLine() {
            return mIsCommandLineDisabled;
        }

        @Override
        public boolean isUploadEnabledForTests() {
            return mIsEnabledForTests;
        }
    }
}
