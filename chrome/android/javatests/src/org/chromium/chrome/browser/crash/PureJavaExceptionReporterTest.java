// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.components.minidump_uploader.CrashTestRule;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;

/**
 * Unittests for {@link PureJavaExceptionReporter}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PureJavaExceptionReporterTest {
    @Rule
    @SuppressFBWarnings("URF_UNREAD_PUBLIC_OR_PROTECTED_FIELD")
    public CrashTestRule mTestRule = new CrashTestRule();

    private static class TestPureJavaExceptionReporter extends PureJavaExceptionReporter {
        boolean mReportUploaded;

        @Override
        public void uploadReport() {
            if (mMinidumpFile == null) {
                mReportUploaded = false;
                return;
            }
            LogcatExtractionRunnable logcatExtractionRunnable =
                    new LogcatExtractionRunnable(mMinidumpFile);
            mMinidumpFile = logcatExtractionRunnable.attachLogcatToMinidump();
            mReportUploaded = true;
        }

        public boolean reportUploaded() {
            return mReportUploaded;
        }
    }

    private static final String EXCEPTION_NAME = "EXCEPTION_NAME";

    private static final String[] REPORT_FIELDS = {PureJavaExceptionReporter.CHANNEL,
            PureJavaExceptionReporter.VERSION, PureJavaExceptionReporter.PRODUCT,
            PureJavaExceptionReporter.ANDROID_BUILD_ID, PureJavaExceptionReporter.ANDROID_BUILD_FP,
            PureJavaExceptionReporter.DEVICE, PureJavaExceptionReporter.GMS_CORE_VERSION,
            PureJavaExceptionReporter.INSTALLER_PACKAGE_NAME, PureJavaExceptionReporter.ABI_NAME,
            PureJavaExceptionReporter.PACKAGE, PureJavaExceptionReporter.MODEL,
            PureJavaExceptionReporter.BRAND, PureJavaExceptionReporter.EXCEPTION_INFO,
            PureJavaExceptionReporter.PROCESS_TYPE, PureJavaExceptionReporter.EARLY_JAVA_EXCEPTION};

    private String readFileToString(File file) {
        StringBuilder sb = new StringBuilder();
        try (BufferedReader br = new BufferedReader(new FileReader(file))) {
            String line;
            while ((line = br.readLine()) != null) {
                sb.append(line);
            }
        } catch (IOException e) {
        }
        return sb.toString();
    }

    @Test
    @SmallTest
    public void verifyMinidumpContentAndUpload() {
        Throwable exception = new RuntimeException(EXCEPTION_NAME);
        TestPureJavaExceptionReporter reporter = new TestPureJavaExceptionReporter();
        reporter.createAndUploadReport(exception);
        String minidumpString = readFileToString(reporter.getMinidumpFile());

        for (String field : REPORT_FIELDS) {
            Assert.assertTrue("Report field \"" + field
                            + "\" is not included in the report. Minidump string is \""
                            + minidumpString + "\"",
                    minidumpString.contains(field));
        }
        // Exception string should be included in the stack trace.
        Assert.assertTrue(minidumpString.contains(EXCEPTION_NAME));

        // Current function name should be included in the stack trace.
        Assert.assertTrue(minidumpString.contains("verifyMinidumpContentAndUpload"));

        Assert.assertTrue(reporter.reportUploaded());
    }
}
