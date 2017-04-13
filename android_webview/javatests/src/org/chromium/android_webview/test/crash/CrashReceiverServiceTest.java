// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.crash;

import android.os.ParcelFileDescriptor;
import android.support.test.filters.MediumTest;
import android.test.InstrumentationTestCase;

import org.chromium.android_webview.crash.CrashReceiverService;
import org.chromium.base.ContextUtils;

import java.io.File;
import java.io.IOException;

/**
 * Instrumentation tests for CrashReceiverService.
 */
public class CrashReceiverServiceTest extends InstrumentationTestCase {
    protected void setUp() throws Exception {
        ContextUtils.initApplicationContextForTests(
                getInstrumentation().getTargetContext().getApplicationContext());
    }

    /**
     * Ensure that the minidump copying doesn't trigger when we pass it invalid file descriptors.
     */
    @MediumTest
    public void testCopyingAbortsForInvalidFds() throws IOException {
        assertFalse(CrashReceiverService.copyMinidumps(0 /* uid */, null));
        assertFalse(CrashReceiverService.copyMinidumps(
                0 /* uid */, new ParcelFileDescriptor[] {null, null}));
        assertFalse(CrashReceiverService.copyMinidumps(0 /* uid */, new ParcelFileDescriptor[0]));
    }

    /**
     * Ensure deleting temporary files used when copying minidumps works correctly.
     */
    @MediumTest
    public void testDeleteFilesInDir() throws IOException {
        File webviewTmpDir = CrashReceiverService.getWebViewTmpCrashDir();
        if (!webviewTmpDir.isDirectory()) {
            assertTrue(webviewTmpDir.mkdir());
        }
        File testFile1 = new File(webviewTmpDir, "testFile1");
        File testFile2 = new File(webviewTmpDir, "testFile2");
        assertTrue(testFile1.createNewFile());
        assertTrue(testFile2.createNewFile());
        assertTrue(testFile1.exists());
        assertTrue(testFile2.exists());
        CrashReceiverService.deleteFilesInWebViewTmpDirIfExists();
        assertFalse(testFile1.exists());
        assertFalse(testFile2.exists());
    }
}
