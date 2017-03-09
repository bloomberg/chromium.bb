// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.support.test.filters.MediumTest;

import org.chromium.base.StreamUtil;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.minidump_uploader.CrashTestCase;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Unittests for {@link LogcatExtractionRunnable}.
 */
public class LogcatExtractionRunnableTest extends CrashTestCase {
    private File mCrashDir;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mCrashDir = new CrashFileManager(mCacheDir).getCrashDirectory();
    }

    @MediumTest
    public void testSimpleExtraction() {
        // Set up the test.
        final List<String> logcat = new ArrayList<String>();
        logcat.add("some random log content");
        logcat.add("some more deterministic log content");

        final File minidump = new File(mCrashDir, "test.dmp");
        final String boundary = "boundary";
        final String minidumpContents = "important minidump contents";
        FileWriter writer = null;
        try {
            writer = new FileWriter(minidump);
            writer.write(boundary + "\n");
            writer.write(minidumpContents + "\n");
        } catch (IOException e) {
            fail(e.toString());
        } finally {
            StreamUtil.closeQuietly(writer);
        }

        // The testContext is responsible for verifying that the correct intent is fired after the
        // logcat is extracted.
        final AtomicInteger numServiceStarts = new AtomicInteger(0);
        Context testContext = new AdvancedMockContext(getInstrumentation().getTargetContext()) {
            @Override
            public ComponentName startService(Intent intent) {
                assertEquals(1, numServiceStarts.incrementAndGet());
                assertEquals(MinidumpUploadService.class.getName(),
                        intent.getComponent().getClassName());
                assertEquals(MinidumpUploadService.ACTION_UPLOAD, intent.getAction());
                assertEquals(new File(mCrashDir, "test.dmp.try0").getAbsolutePath(),
                        intent.getStringExtra(MinidumpUploadService.FILE_TO_UPLOAD_KEY));
                return super.startService(intent);
            }
        };

        // Extract the logcat!
        LogcatExtractionRunnable runnable = new LogcatExtractionRunnable(testContext, minidump) {
            @Override
            protected List<String> getLogcat() {
                return logcat;
            }
        };
        runnable.run();

        // Verify the file contents.
        BufferedReader input = null;
        try {
            File logfile = new File(mCrashDir, "test.dmp.try0");
            assertTrue("Logfile should exist!", logfile.exists());

            input = new BufferedReader(new FileReader(logfile));
            assertEquals("The first line should be the boundary line.", boundary, input.readLine());
            assertEquals("The second line should be the content dispoistion.",
                    MinidumpLogcatPrepender.LOGCAT_CONTENT_DISPOSITION, input.readLine());
            assertEquals("The third line should be the content type.",
                    MinidumpLogcatPrepender.LOGCAT_CONTENT_TYPE, input.readLine());
            assertEquals("The fourth line should be blank, for padding.", "", input.readLine());
            for (String expected : logcat) {
                assertEquals(expected, input.readLine());
            }
            assertEquals("The logcat should be followed by the boundary line.", boundary,
                    input.readLine());
            assertEquals(
                    "The minidump contents should follow.", minidumpContents, input.readLine());
            assertNull("There should be nothing else in the file", input.readLine());
        } catch (IOException e) {
            fail(e.toString());
        } finally {
            StreamUtil.closeQuietly(input);
        }
    }
}
