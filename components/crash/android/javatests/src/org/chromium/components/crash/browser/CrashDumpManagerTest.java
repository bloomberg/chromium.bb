// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.crash.browser;

import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;

import java.io.File;
import java.io.IOException;

/**
 * Unittests for {@link CrashDumpManager}.
 */
public class CrashDumpManagerTest extends InstrumentationTestCase {
    File mTempDir;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        ContextUtils.initApplicationContextForTests(
                getInstrumentation().getTargetContext().getApplicationContext());
        mTempDir = new File(ContextUtils.getApplicationContext().getCacheDir(), "crash-dump-test");
        if (mTempDir.exists()) {
            FileUtils.recursivelyDeleteFile(mTempDir);
        }
        assert mTempDir.mkdirs();
    }

    @Override
    protected void tearDown() throws Exception {
        FileUtils.recursivelyDeleteFile(mTempDir);
        super.tearDown();
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUploadMinidump_NoCallback() throws IOException {
        File minidump = new File(mTempDir, "mini.dmp");
        assertTrue(minidump.createNewFile());

        CrashDumpManager.tryToUploadMinidump(minidump.getAbsolutePath());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUploadMinidump_NullMinidumpPath() {
        registerUploadCallback(new CrashDumpManager.UploadMinidumpCallback() {
            @Override
            public void tryToUploadMinidump(File minidump) {
                fail("The callback should not be called when the minidump path is null.");
            }
        });

        CrashDumpManager.tryToUploadMinidump(null);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUploadMinidump_FileDoesntExist() {
        registerUploadCallback(new CrashDumpManager.UploadMinidumpCallback() {
            @Override
            public void tryToUploadMinidump(File minidump) {
                fail("The callback should not be called when the minidump file doesn't exist.");
            }
        });

        CrashDumpManager.tryToUploadMinidump(
                mTempDir.getAbsolutePath() + "/some/file/that/doesnt/exist");
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUploadMinidump_MinidumpPathIsDirectory() throws IOException {
        File directory = new File(mTempDir, "a_directory");
        assertTrue(directory.mkdir());

        registerUploadCallback(new CrashDumpManager.UploadMinidumpCallback() {
            @Override
            public void tryToUploadMinidump(File minidump) {
                fail("The callback should not be called when the minidump path is not a file.");
            }
        });

        CrashDumpManager.tryToUploadMinidump(directory.getAbsolutePath());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUploadMinidump_MinidumpPathIsValid() throws IOException {
        final File minidump = new File(mTempDir, "mini.dmp");
        assertTrue(minidump.createNewFile());

        registerUploadCallback(new CrashDumpManager.UploadMinidumpCallback() {
            @Override
            public void tryToUploadMinidump(File actualMinidump) {
                assertEquals("Should call the callback with the correct filename.", minidump,
                        actualMinidump);
            }
        });

        CrashDumpManager.tryToUploadMinidump(minidump.getAbsolutePath());
    }

    /**
     * A convenience wrapper that registers the upload {@param callback}, running the registration
     * on the UI thread, as expected by the CrashDumpManager code.
     * @param callback The callback for uploading minidumps.
     */
    private static void registerUploadCallback(
            final CrashDumpManager.UploadMinidumpCallback callback) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                CrashDumpManager.registerUploadCallback(callback);
            }
        });
    }
}
