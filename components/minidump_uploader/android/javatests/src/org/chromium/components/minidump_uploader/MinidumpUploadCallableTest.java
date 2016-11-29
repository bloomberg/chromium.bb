// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.util.HttpURLConnectionFactory;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Unittests for {@link MinidumpUploadCallable}.
 */
public class MinidumpUploadCallableTest extends CrashTestCase {
    private static final String BOUNDARY = "TESTBOUNDARY";
    private static final String CRASH_ID = "IMACRASHID";
    private static final String LOG_FILE_NAME = "chromium_renderer-123_log.dmp224";
    private File mTestUpload;
    private File mUploadLog;
    private File mExpectedFileAfterUpload;

    private static class TestHttpURLConnection extends HttpURLConnection {
        private static final String EXPECTED_CONTENT_TYPE_VALUE =
                String.format(MinidumpUploadCallable.CONTENT_TYPE_TMPL, BOUNDARY);

        /**
         * The value of the "Content-Type" property if the property has been set.
         */
        private String mContentTypePropertyValue = "";

        TestHttpURLConnection(URL url) {
            super(url);
            assertEquals(MinidumpUploadCallable.CRASH_URL_STRING, url.toString());
        }

        @Override
        public void disconnect() {
            // Check that the "Content-Type" property has been set and the property's value.
            assertEquals(EXPECTED_CONTENT_TYPE_VALUE, mContentTypePropertyValue);
        }

        @Override
        public InputStream getInputStream() {
            return new ByteArrayInputStream(CRASH_ID.getBytes());
        }

        @Override
        public OutputStream getOutputStream() {
            return new ByteArrayOutputStream();
        }

        @Override
        public int getResponseCode() {
            return 200;
        }

        @Override
        public String getResponseMessage() {
            return null;
        }

        @Override
        public boolean usingProxy() {
            return false;
        }

        @Override
        public void connect() {
        }

        @Override
        public void setRequestProperty(String key, String value) {
            if (key.equals("Content-Type")) {
                mContentTypePropertyValue = value;
            }
        }
    }

    private static class TestHttpURLConnectionFactory implements HttpURLConnectionFactory {
        @Override
        public HttpURLConnection createHttpURLConnection(String url) {
            try {
                return new TestHttpURLConnection(new URL(url));
            } catch (IOException e) {
                return null;
            }
        }
    }

    private static class FailHttpURLConnectionFactory implements HttpURLConnectionFactory {
        @Override
        public HttpURLConnection createHttpURLConnection(String url) {
            fail();
            return null;
        }
    }

    /**
     * This class calls |getInstrumentation| which cannot be done in a static context.
     */
    private class MockMinidumpUploadCallable extends MinidumpUploadCallable {
        MockMinidumpUploadCallable(
                HttpURLConnectionFactory httpURLConnectionFactory,
                CrashReportingPermissionManager permManager) {
            super(mTestUpload, mUploadLog, httpURLConnectionFactory, permManager);
        }
    }

    private void createMinidumpFile() throws Exception {
        mTestUpload = new File(mCrashDir, LOG_FILE_NAME);
        setUpMinidumpFile(mTestUpload, BOUNDARY);
    }

    @SuppressFBWarnings("RV_RETURN_VALUE_IGNORED_BAD_PRACTICE")
    private void setForcedUpload() throws Exception {
        File renamed = new File(mCrashDir, mTestUpload.getName().replace(".dmp", ".forced"));
        mTestUpload.renameTo(renamed);
        // Update the filename that tests will refer to.
        mTestUpload = renamed;
    }

    @SuppressFBWarnings("RV_RETURN_VALUE_IGNORED_BAD_PRACTICE")
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mUploadLog = new File(mCrashDir, CrashFileManager.CRASH_DUMP_LOGFILE);
        // Delete all logs from previous runs if possible.
        mUploadLog.delete();

        // Any created files will be cleaned up as part of CrashTestCase::tearDown().
        createMinidumpFile();
        mExpectedFileAfterUpload =
                new File(mCrashDir, mTestUpload.getName().replace(".dmp", ".up"));
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallWhenCurrentlyPermitted() throws Exception {
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsPermitted = true;
                        mIsUserPermitted = true;
                        mIsCommandLineDisabled = false;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        assertEquals(MinidumpUploadCallable.UPLOAD_SUCCESS,
                minidumpUploadCallable.call().intValue());
        assertTrue(mExpectedFileAfterUpload.exists());
        assertValidUploadLogEntry();
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallNotPermittedByUser() throws Exception {
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsPermitted = false;
                        mIsUserPermitted = false;
                        mIsCommandLineDisabled = false;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        HttpURLConnectionFactory httpURLConnectionFactory = new FailHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        assertEquals(MinidumpUploadCallable.UPLOAD_USER_DISABLED,
                minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload =
                new File(mCrashDir, mTestUpload.getName().replace(".dmp", ".skipped"));
        assertTrue(expectedSkippedFileAfterUpload.exists());
        assertFalse(mExpectedFileAfterUpload.exists());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallNotPermittedByCommandLine() throws Exception {
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsPermitted = true;
                        mIsUserPermitted = true;
                        mIsCommandLineDisabled = true;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        HttpURLConnectionFactory httpURLConnectionFactory = new FailHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        assertEquals(MinidumpUploadCallable.UPLOAD_COMMANDLINE_DISABLED,
                minidumpUploadCallable.call().intValue());
        assertFalse(mExpectedFileAfterUpload.exists());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallPermittedButNotInSample() throws Exception {
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = false;
                        mIsPermitted = true;
                        mIsUserPermitted = true;
                        mIsCommandLineDisabled = false;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        assertEquals(MinidumpUploadCallable.UPLOAD_DISABLED_BY_SAMPLING,
                minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload =
                new File(mCrashDir, mTestUpload.getName().replace(".dmp", ".skipped"));
        assertTrue(expectedSkippedFileAfterUpload.exists());
        assertFalse(mExpectedFileAfterUpload.exists());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallPermittedButNotUnderCurrentCircumstances() throws Exception {
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsPermitted = true;
                        mIsUserPermitted = true;
                        mIsCommandLineDisabled = false;
                        mIsNetworkAvailable = false;
                        mIsEnabledForTests = false;
                    }
                };

        HttpURLConnectionFactory httpURLConnectionFactory = new FailHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        assertEquals(MinidumpUploadCallable.UPLOAD_FAILURE,
                minidumpUploadCallable.call().intValue());
        assertFalse(mExpectedFileAfterUpload.exists());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCrashUploadEnabledForTestsDespiteConstraints() throws Exception {
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsPermitted = false;
                        mIsUserPermitted = false;
                        mIsCommandLineDisabled = false;
                        mIsNetworkAvailable = false;
                        mIsEnabledForTests = true;
                    }
                };

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        assertEquals(MinidumpUploadCallable.UPLOAD_SUCCESS,
                minidumpUploadCallable.call().intValue());
        assertTrue(mExpectedFileAfterUpload.exists());
        assertValidUploadLogEntry();
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallWhenCurrentlyPermitted_ForcedUpload() throws Exception {
        setForcedUpload();
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsPermitted = true;
                        mIsUserPermitted = true;
                        mIsCommandLineDisabled = false;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        assertEquals(
                MinidumpUploadCallable.UPLOAD_SUCCESS, minidumpUploadCallable.call().intValue());
        assertTrue(mExpectedFileAfterUpload.exists());
        assertValidUploadLogEntry();
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallNotPermittedByUser_ForcedUpload() throws Exception {
        setForcedUpload();
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsPermitted = false;
                        mIsUserPermitted = false;
                        mIsCommandLineDisabled = false;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        assertEquals(
                MinidumpUploadCallable.UPLOAD_SUCCESS, minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload =
                new File(mCrashDir, mTestUpload.getName().replace(".forced", ".skipped"));
        assertFalse(expectedSkippedFileAfterUpload.exists());
        assertTrue(mExpectedFileAfterUpload.exists());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallNotPermittedByCommandLine_ForcedUpload() throws Exception {
        setForcedUpload();
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsPermitted = true;
                        mIsUserPermitted = true;
                        mIsCommandLineDisabled = true;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        HttpURLConnectionFactory httpURLConnectionFactory = new FailHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        assertEquals(MinidumpUploadCallable.UPLOAD_COMMANDLINE_DISABLED,
                minidumpUploadCallable.call().intValue());
        assertFalse(mExpectedFileAfterUpload.exists());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallPermittedButNotInSample_ForcedUpload() throws Exception {
        setForcedUpload();
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = false;
                        mIsPermitted = true;
                        mIsUserPermitted = true;
                        mIsCommandLineDisabled = false;
                        mIsNetworkAvailable = true;
                        mIsEnabledForTests = false;
                    }
                };

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        assertEquals(
                MinidumpUploadCallable.UPLOAD_SUCCESS, minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload =
                new File(mCrashDir, mTestUpload.getName().replace(".forced", ".skipped"));
        assertFalse(expectedSkippedFileAfterUpload.exists());
        assertTrue(mExpectedFileAfterUpload.exists());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallPermittedButNotUnderCurrentCircumstances_ForcedUpload() throws Exception {
        setForcedUpload();
        CrashReportingPermissionManager testPermManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsPermitted = true;
                        mIsUserPermitted = true;
                        mIsCommandLineDisabled = false;
                        mIsNetworkAvailable = false;
                        mIsEnabledForTests = false;
                    }
                };

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory();

        MinidumpUploadCallable minidumpUploadCallable =
                new MockMinidumpUploadCallable(httpURLConnectionFactory, testPermManager);
        assertEquals(
                MinidumpUploadCallable.UPLOAD_SUCCESS, minidumpUploadCallable.call().intValue());

        File expectedSkippedFileAfterUpload =
                new File(mCrashDir, mTestUpload.getName().replace(".forced", ".skipped"));
        assertFalse(expectedSkippedFileAfterUpload.exists());
        assertTrue(mExpectedFileAfterUpload.exists());
    }

    private void extendUploadFile(int numBytes) throws FileNotFoundException, IOException {
        FileOutputStream stream = null;
        try {
            stream = new FileOutputStream(mTestUpload, true);
            byte[] buf = new byte[numBytes];
            stream.write(buf);
            stream.flush();
        } finally {
            if (stream != null) stream.close();
        }
    }

    private void assertValidUploadLogEntry() throws IOException {
        File logfile = new File(mCrashDir, CrashFileManager.CRASH_DUMP_LOGFILE);
        BufferedReader input =  new BufferedReader(new FileReader(logfile));
        String line = null;
        String lastEntry = null;
        while ((line = input.readLine()) != null) {
            lastEntry = line;
        }
        input.close();

        assertNotNull("We do not have a single entry in uploads.log", lastEntry);
        int seperator = lastEntry.indexOf(',');

        long time = Long.parseLong(lastEntry.substring(0, seperator));
        long now = System.currentTimeMillis() / 1000; // Timestamp was in seconds.

        // Sanity check on the time stamp (within an hour).
        // Chances are the write and the check should have less than 1 second in between.
        assertTrue(time <= now);
        assertTrue(time > now - 60 * 60);

        String id = lastEntry.substring(seperator + 1, lastEntry.length());
        assertEquals(id, CRASH_ID);
    }
}
