// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.annotation.TargetApi;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.support.test.filters.MediumTest;

import org.chromium.base.StreamUtil;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.minidump_uploader.CrashTestCase;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Unittests for {@link LogcatExtractionRunnable}.
 */
public class LogcatExtractionRunnableTest extends CrashTestCase {
    private File mCrashDir;

    private static final String BOUNDARY = "boundary";
    private static final String MINIDUMP_CONTENTS = "important minidump contents";
    private static final List<String> LOGCAT =
            Arrays.asList("some random log content", "some more deterministic log content");

    private static class TestLogcatExtractionRunnable extends LogcatExtractionRunnable {
        TestLogcatExtractionRunnable(Context context, File minidump) {
            super(context, minidump);
        }

        @Override
        protected List<String> getLogcat() {
            return LOGCAT;
        }
    };

    @TargetApi(Build.VERSION_CODES.M)
    private static class TestJobScheduler extends JobScheduler {
        TestJobScheduler() {}

        @Override
        public void cancel(int jobId) {}

        @Override
        public void cancelAll() {}

        @Override
        public List<JobInfo> getAllPendingJobs() {
            return null;
        }

        @Override
        public JobInfo getPendingJob(int jobId) {
            return null;
        }

        @Override
        public int schedule(JobInfo job) {
            assertEquals(TaskIds.CHROME_MINIDUMP_UPLOADING_JOB_ID, job.getId());
            assertEquals(ChromeMinidumpUploadJobService.class.getName(),
                    job.getService().getClassName());
            return JobScheduler.RESULT_SUCCESS;
        }
    };

    // Responsible for verifying that the correct intent is fired after the logcat is extracted.
    private class TestContext extends AdvancedMockContext {
        int mNumServiceStarts;

        TestContext(Context realContext) {
            super(realContext);
        }

        @Override
        public ComponentName startService(Intent intent) {
            assertFalse("Should only start a service directly when the job scheduler is disabled.",
                    ChromeFeatureList.isEnabled(
                            ChromeFeatureList.UPLOAD_CRASH_REPORTS_USING_JOB_SCHEDULER));
            ++mNumServiceStarts;
            assertEquals(1, mNumServiceStarts);
            assertEquals(
                    MinidumpUploadService.class.getName(), intent.getComponent().getClassName());
            assertEquals(MinidumpUploadService.ACTION_UPLOAD, intent.getAction());
            assertEquals(new File(mCrashDir, "test.dmp.try0").getAbsolutePath(),
                    intent.getStringExtra(MinidumpUploadService.FILE_TO_UPLOAD_KEY));
            return super.startService(intent);
        }

        @Override
        public Object getSystemService(String name) {
            if (Context.JOB_SCHEDULER_SERVICE.equals(name)) {
                assertTrue("Should only access the JobScheduler when it is enabled.",
                        ChromeFeatureList.isEnabled(
                                ChromeFeatureList.UPLOAD_CRASH_REPORTS_USING_JOB_SCHEDULER));
                return new TestJobScheduler();
            }

            return super.getSystemService(name);
        }
    };

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mCrashDir = new CrashFileManager(mCacheDir).getCrashDirectory();
    }

    @Override
    protected void tearDown() throws Exception {
        ChromeFeatureList.setTestFeatures(null);
        super.tearDown();
    }

    /**
     * Sets whether to upload minidumps using the JobScheduler API. Minidumps can either be uploaded
     * via a JobScheduler, or via a direct Intent service.
     * @param enable Whether to enable the JobScheduler API.
     */
    private void setJobSchedulerEnabled(boolean enable) {
        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.UPLOAD_CRASH_REPORTS_USING_JOB_SCHEDULER, enable);
        ChromeFeatureList.setTestFeatures(features);
    }

    /**
     * Creates a simple fake minidump file for testing.
     * @param filename The name of the file to create.
     */
    private File createMinidump(String filename) throws IOException {
        File minidump = new File(mCrashDir, filename);
        FileWriter writer = null;
        try {
            writer = new FileWriter(minidump);
            writer.write(BOUNDARY + "\n");
            writer.write(MINIDUMP_CONTENTS + "\n");
        } finally {
            StreamUtil.closeQuietly(writer);
        }
        return minidump;
    }

    /**
     * Verifies that the contents of the {@param filename} are the expected ones.
     * @param filename The name of the file containing the concatenated logcat and minidump output.
     */
    private void verifyMinidumpWithLogcat(String filename) throws IOException {
        BufferedReader input = null;
        try {
            File minidumpWithLogcat = new File(mCrashDir, filename);
            assertTrue("Should have created a file containing the logcat and minidump contents",
                    minidumpWithLogcat.exists());

            input = new BufferedReader(new FileReader(minidumpWithLogcat));
            assertEquals("The first line should be the boundary line.", BOUNDARY, input.readLine());
            assertEquals("The second line should be the content dispoistion.",
                    MinidumpLogcatPrepender.LOGCAT_CONTENT_DISPOSITION, input.readLine());
            assertEquals("The third line should be the content type.",
                    MinidumpLogcatPrepender.LOGCAT_CONTENT_TYPE, input.readLine());
            assertEquals("The fourth line should be blank, for padding.", "", input.readLine());
            for (String expected : LOGCAT) {
                assertEquals("The logcat contents should match", expected, input.readLine());
            }
            assertEquals("The logcat should be followed by the boundary line.", BOUNDARY,
                    input.readLine());
            assertEquals(
                    "The minidump contents should follow.", MINIDUMP_CONTENTS, input.readLine());
            assertNull("There should be nothing else in the file", input.readLine());
        } finally {
            StreamUtil.closeQuietly(input);
        }
    }

    @MediumTest
    public void testSimpleExtraction_SansJobScheduler() throws IOException {
        setJobSchedulerEnabled(false);
        final File minidump = createMinidump("test.dmp");
        Context testContext = new TestContext(getInstrumentation().getTargetContext());

        LogcatExtractionRunnable runnable = new TestLogcatExtractionRunnable(testContext, minidump);
        runnable.run();

        verifyMinidumpWithLogcat("test.dmp.try0");
    }

    @MediumTest
    public void testSimpleExtraction_WithJobScheduler() throws IOException {
        // The JobScheduler API is only available as of Android M.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;

        setJobSchedulerEnabled(true);
        final File minidump = createMinidump("test.dmp");
        Context testContext = new TestContext(getInstrumentation().getTargetContext());

        LogcatExtractionRunnable runnable = new TestLogcatExtractionRunnable(testContext, minidump);
        runnable.run();

        verifyMinidumpWithLogcat("test.dmp.try0");
    }
}
