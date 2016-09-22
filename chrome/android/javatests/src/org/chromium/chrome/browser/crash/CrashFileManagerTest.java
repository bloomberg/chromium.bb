// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.test.MoreAsserts;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.Date;
import java.util.concurrent.TimeUnit;
import java.util.regex.Pattern;

/**
 * Unittests for {@link CrashFileManager}.
 */
public class CrashFileManagerTest extends CrashTestCase {
    private static final int TEST_PID = 23;

    private long mInitialModificationTimestamp;
    private long mModificationTimestamp;

    private File mTmpFile1;
    private File mTmpFile2;
    private File mTmpFile3;

    private File mDmpFile1;
    private File mDmpFile2;

    private File mUpFile1;
    private File mUpFile2;

    private File mLogfile;

    @SuppressFBWarnings("RV_RETURN_VALUE_IGNORED_BAD_PRACTICE")
    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mInitialModificationTimestamp = new Date().getTime();
        mModificationTimestamp = mInitialModificationTimestamp;

        // The following files will be deleted in CrashTestCase#tearDown().
        mTmpFile1 = new File(mCrashDir, "12345ABCDE" + CrashFileManager.TMP_SUFFIX);
        mTmpFile1.createNewFile();
        mTmpFile1.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mTmpFile2 = new File(mCrashDir, "abcde12345" + CrashFileManager.TMP_SUFFIX);
        mTmpFile2.createNewFile();
        mTmpFile2.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mTmpFile3 = new File(mCrashDir, "abcdefghi" + CrashFileManager.TMP_SUFFIX);
        mTmpFile3.createNewFile();
        mTmpFile3.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mDmpFile1 = new File(mCrashDir, "123_abc.dmp0");
        mDmpFile1.createNewFile();
        mDmpFile1.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mDmpFile2 = new File(mCrashDir, "chromium-renderer_abc.dmp" + TEST_PID);
        mDmpFile2.createNewFile();
        mDmpFile2.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mUpFile1 = new File(mCrashDir, "123_abcd.up0");
        mUpFile1.createNewFile();
        mUpFile1.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mUpFile2 = new File(mCrashDir, "chromium-renderer_abcd.up" + TEST_PID);
        mUpFile2.createNewFile();
        mUpFile2.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mLogfile = new File(mCrashDir, CrashFileManager.CRASH_DUMP_LOGFILE);
        mLogfile.createNewFile();
        mLogfile.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCrashFileManagerWithNull() {
        try {
            new CrashFileManager(null);
            fail("Constructor should throw NullPointerException with null context.");
        } catch (NullPointerException npe) {
            return;
        }
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetMatchingFiles() {
        CrashFileManager crashFileManager = new CrashFileManager(mCacheDir);
        // Three files begin with 123.
        File[] expectedFiles = new File[] { mTmpFile1, mDmpFile1, mUpFile1 };
        Pattern testPattern = Pattern.compile("^123");
        File[] actualFiles = crashFileManager.getMatchingFiles(testPattern);
        assertNotNull(actualFiles);
        MoreAsserts.assertEquals("Failed to match file by pattern", expectedFiles, actualFiles);
    }

    @MediumTest
    @Feature({"Android-AppBase"})
    public void testFileComparator() throws IOException {
        CrashFileManager crashFileManager = new CrashFileManager(mCacheDir);
        File[] expectedFiles = new File[] {mTmpFile3, mTmpFile2, mTmpFile1};
        File[] originalFiles = new File[] {mTmpFile1, mTmpFile2, mTmpFile3};
        Arrays.sort(originalFiles, crashFileManager.sFileComparator);
        assertNotNull(originalFiles);
        MoreAsserts.assertEquals("File comparator failed to prioritize last modified file",
                expectedFiles, originalFiles);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetAllMinidumpFilesSorted() {
        CrashFileManager crashFileManager = new CrashFileManager(mCacheDir);
        File[] expectedFiles = new File[] {mDmpFile2, mDmpFile1};
        File[] actualFiles = crashFileManager.getAllMinidumpFilesSorted();
        assertNotNull(actualFiles);
        MoreAsserts.assertEquals("Failed to sort minidumps by modification time", expectedFiles,
                actualFiles);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetAllFilesSorted() {
        CrashFileManager crashFileManager = new CrashFileManager(mCacheDir);
        File[] expectedFiles = new File[] {mLogfile, mUpFile2, mUpFile1, mDmpFile2, mDmpFile1,
                mTmpFile3, mTmpFile2, mTmpFile1};
        File[] actualFiles = crashFileManager.getAllFilesSorted();
        assertNotNull(actualFiles);
        MoreAsserts.assertEquals(
                "Failed to sort all files by modification time", expectedFiles, actualFiles);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetCrashDirectory() {
        CrashFileManager crashFileManager = new CrashFileManager(mCacheDir);
        File actualFile = crashFileManager.getCrashDirectory();
        assertTrue(actualFile.isDirectory());
        assertEquals(mCrashDir, actualFile);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testDeleteFile() {
        assertTrue(mTmpFile1.exists());
        assertTrue(CrashFileManager.deleteFile(mTmpFile1));
        assertFalse(mTmpFile1.exists());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetAllMinidumpFiles() {
        CrashFileManager crashFileManager = new CrashFileManager(mCacheDir);
        File[] expectedFiles = new File[] { mDmpFile1, mDmpFile2 };
        File[] actualFiles = crashFileManager.getAllMinidumpFiles();
        assertNotNull(actualFiles);
        MoreAsserts.assertEquals("Failed to get the correct minidump files in directory",
                expectedFiles, actualFiles);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetAllUploadedFiles() {
        CrashFileManager crashFileManager = new CrashFileManager(mCacheDir);
        File[] expectedFiles = new File[] { mUpFile1, mUpFile2 };
        File[] actualFiles = crashFileManager.getAllUploadedFiles();
        assertNotNull(actualFiles);
        MoreAsserts.assertEquals("Failed to get the correct uploaded files in directory",
                expectedFiles, actualFiles);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testAttemptNumber() {
        assertEquals(0, CrashFileManager.readAttemptNumber("file.dmp"));
        assertEquals(0, CrashFileManager.readAttemptNumber(".try"));
        assertEquals(0, CrashFileManager.readAttemptNumber("try1"));
        assertEquals(1, CrashFileManager.readAttemptNumber("file.try1.dmp"));
        assertEquals(1, CrashFileManager.readAttemptNumber("file.dmp.try1"));
        assertEquals(2, CrashFileManager.readAttemptNumber(".try2"));
        assertEquals(2, CrashFileManager.readAttemptNumber("file.try2.dmp"));
        assertEquals(2, CrashFileManager.readAttemptNumber("file.dmp.try2"));
        assertEquals(2, CrashFileManager.readAttemptNumber(".try2"));
        assertEquals(0, CrashFileManager.readAttemptNumber("file.tryN.dmp"));
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testAttemptNumberRename() {
        assertEquals("file.dmp.try1",
                CrashFileManager.filenameWithIncrementedAttemptNumber("file.dmp"));
        assertEquals("f.dmp.try2",
                CrashFileManager.filenameWithIncrementedAttemptNumber("f.dmp.try1"));
        assertEquals("f.dmp.try20",
                CrashFileManager.filenameWithIncrementedAttemptNumber("f.dmp.try10"));
        assertEquals("f.try2.dmp",
                CrashFileManager.filenameWithIncrementedAttemptNumber("f.try1.dmp"));
        assertEquals("f.tryN.dmp.try1",
                CrashFileManager.filenameWithIncrementedAttemptNumber("f.tryN.dmp"));
        assertEquals("f.forced.try3",
                CrashFileManager.filenameWithIncrementedAttemptNumber("f.forced.try2"));
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testFilenameWithForcedUploadState() {
        // The ".try0" suffix is sometimes implicit -- in particular, when logcat extraction fails.
        assertEquals("file.forced", CrashFileManager.filenameWithForcedUploadState("file.dmp"));
        // A not-yet-attempted upload.
        assertEquals("file.forced0.try0",
                CrashFileManager.filenameWithForcedUploadState("file.dmp0.try0"));
        // A failed upload.
        assertEquals("file.forced12.try0",
                CrashFileManager.filenameWithForcedUploadState("file.dmp12.try3"));

        // The same set of tests as above, but for skipped uploads rather than failed or
        // not-yet-attempted uploads.
        assertEquals("file.forced", CrashFileManager.filenameWithForcedUploadState("file.skipped"));
        assertEquals("file.forced0.try0",
                CrashFileManager.filenameWithForcedUploadState("file.skipped0.try0"));
        assertEquals("file.forced12.try0",
                CrashFileManager.filenameWithForcedUploadState("file.skipped12.try3"));

        // A failed previously-forced upload.
        assertEquals("file.forced0.try0",
                CrashFileManager.filenameWithForcedUploadState("file.forced0.try3"));
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testMarkUploadSuccess() {
        CrashFileManager.markUploadSuccess(mDmpFile1);
        assertFalse(mDmpFile1.exists());
        assertTrue(new File(mCrashDir, "123_abc.up0").exists());
    }

    @SuppressFBWarnings("RV_RETURN_VALUE_IGNORED_BAD_PRACTICE")
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testMarkUploadSuccess_ForcedUpload() throws IOException {
        File forced = new File(mCrashDir, "123_abc.forced0");
        forced.createNewFile();
        CrashFileManager.markUploadSuccess(forced);
        assertFalse(forced.exists());
        assertTrue(new File(mCrashDir, "123_abc.up0").exists());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testMarkUploadSkipped() {
        CrashFileManager.markUploadSkipped(mDmpFile1);
        assertFalse(mDmpFile1.exists());
        assertTrue(new File(mCrashDir, "123_abc.skipped0").exists());
    }

    @SuppressFBWarnings("RV_RETURN_VALUE_IGNORED_BAD_PRACTICE")
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCleanOutAllNonFreshMinidumpFiles() throws IOException {
        // Create some simulated old files.
        long oldTimestamp = mInitialModificationTimestamp
                - TimeUnit.MILLISECONDS.convert(31, TimeUnit.DAYS);
        File old1 = new File(mCrashDir, "chromium-renderer-minidump-cooo10ff.dmp");
        File old2 = new File(mCrashDir, "chromium-renderer-minidump-cooo10ff.up0");
        File old3 = new File(mCrashDir, "chromium-renderer-minidump-cooo10ff.logcat");
        old1.setLastModified(oldTimestamp);
        old2.setLastModified(oldTimestamp - 1);
        old3.setLastModified(oldTimestamp - 2);

        // These will be the most recent files in the directory, after all successfully uploaded
        // files and all temp files are removed.
        File[] recentFiles = new File[3 * CrashFileManager.MAX_CRASH_REPORTS_TO_KEEP];
        for (int i = 0; i < CrashFileManager.MAX_CRASH_REPORTS_TO_KEEP; ++i) {
            String prefix = "chromium-renderer-minidump-deadbeef" + i;
            // There is no reason why both a minidump and failed upload should exist at the same
            // time, but the cleanup code should be robust to it anyway.
            File recentMinidump = new File(mCrashDir, prefix + ".dmp");
            File recentFailedUpload = new File(mCrashDir, prefix + ".up0.try0");
            File recentLogcatFile = new File(mCrashDir, prefix + ".logcat");
            recentMinidump.createNewFile();
            recentFailedUpload.createNewFile();
            recentLogcatFile.createNewFile();
            recentMinidump.setLastModified(mModificationTimestamp);
            mModificationTimestamp += 1000;
            recentFailedUpload.setLastModified(mModificationTimestamp);
            mModificationTimestamp += 1000;
            recentLogcatFile.setLastModified(mModificationTimestamp);
            mModificationTimestamp += 1000;
            recentFiles[3 * i + 0] = recentMinidump;
            recentFiles[3 * i + 1] = recentFailedUpload;
            recentFiles[3 * i + 2] = recentLogcatFile;
        }

        // Create some additional successful uploads.
        File success1 = new File(mCrashDir, "chromium-renderer-minidump-cafebebe1.up0");
        File success2 = new File(mCrashDir, "chromium-renderer-minidump-cafebebe2.up1");
        File success3 = new File(mCrashDir, "chromium-renderer-minidump-cafebebe3.up2");
        success1.createNewFile();
        success2.createNewFile();
        success3.createNewFile();
        success1.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;
        success2.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;
        success3.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        // Create some additional temp files.
        File temp1 = new File(mCrashDir, "chromium-renderer-minidump-oooff1ce1.tmp");
        File temp2 = new File(mCrashDir, "chromium-renderer-minidump-oooff1ce2.tmp");
        temp1.createNewFile();
        temp2.createNewFile();
        temp1.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;
        temp2.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        CrashFileManager crashFileManager = new CrashFileManager(mCacheDir);
        crashFileManager.cleanOutAllNonFreshMinidumpFiles();

        assertFalse(old1.exists());
        assertFalse(old2.exists());
        assertFalse(old3.exists());
        for (File f : recentFiles) {
            assertTrue(f.exists());
        }
        assertTrue(mLogfile.exists());
        assertFalse(mTmpFile1.exists());
        assertFalse(mTmpFile2.exists());
        assertFalse(mTmpFile3.exists());
        assertFalse(mDmpFile1.exists());
        assertFalse(mDmpFile2.exists());
        assertFalse(mUpFile1.exists());
        assertFalse(mUpFile2.exists());
        assertFalse(temp1.exists());
        assertFalse(temp2.exists());
        assertFalse(success1.exists());
        assertFalse(success2.exists());
        assertFalse(success3.exists());
    }
}
