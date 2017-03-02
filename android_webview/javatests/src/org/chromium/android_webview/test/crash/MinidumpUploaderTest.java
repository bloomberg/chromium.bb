// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.crash;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.content.Context;
import android.os.ParcelFileDescriptor;
import android.support.test.filters.MediumTest;
import android.webkit.ValueCallback;

import org.chromium.android_webview.PlatformServiceBridge;
import org.chromium.android_webview.crash.AwMinidumpUploaderDelegate;
import org.chromium.android_webview.crash.CrashReceiverService;
import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.minidump_uploader.CrashTestCase;
import org.chromium.components.minidump_uploader.MinidumpUploadCallable;
import org.chromium.components.minidump_uploader.MinidumpUploadCallableTest;
import org.chromium.components.minidump_uploader.MinidumpUploader;
import org.chromium.components.minidump_uploader.MinidumpUploaderDelegate;
import org.chromium.components.minidump_uploader.MinidumpUploaderImpl;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.util.HttpURLConnectionFactory;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Instrumentation tests for MinidumpUploader.
 */
public class MinidumpUploaderTest extends CrashTestCase {
    private static final String TAG = "MinidumpUploaderTest";
    private static final String BOUNDARY = "TESTBOUNDARY";

    private static final long TIME_OUT_MILLIS = 3000;

    @Override
    protected File getExistingCacheDir() {
        return CrashReceiverService.createWebViewCrashDir(getInstrumentation().getTargetContext());
    }

    private static class TestPlatformServiceBridge extends PlatformServiceBridge {
        boolean mCanUseGms;
        boolean mUserPermitted;

        public TestPlatformServiceBridge(boolean canUseGms, boolean userPermitted) {
            mCanUseGms = canUseGms;
            mUserPermitted = userPermitted;
        }

        @Override
        public boolean canUseGms() {
            return mCanUseGms;
        }

        @Override
        public void queryMetricsSetting(ValueCallback<Boolean> callback) {
            ThreadUtils.assertOnUiThread();
            callback.onReceiveValue(mUserPermitted);
        }
    }

    private static class TestMinidumpUploaderDelegate extends AwMinidumpUploaderDelegate {
        private final CrashReportingPermissionManager mPermissionManager;

        TestMinidumpUploaderDelegate(
                Context context, CrashReportingPermissionManager permissionManager) {
            super(context);
            mPermissionManager = permissionManager;
        }

        @Override
        public PlatformServiceBridge createPlatformServiceBridge() {
            return new TestPlatformServiceBridge(true /* canUseGms */,
                    mPermissionManager.isUsageAndCrashReportingPermittedByUser());
        }
    }

    private static class TestMinidumpUploaderImpl extends MinidumpUploaderImpl {
        private final CrashReportingPermissionManager mPermissionManager;

        TestMinidumpUploaderImpl(MinidumpUploaderDelegate delegate,
                CrashReportingPermissionManager permissionManager) {
            super(delegate);
            mPermissionManager = permissionManager;
        }

        @Override
        public CrashFileManager createCrashFileManager(File crashDir) {
            return new CrashFileManager(crashDir) {
                @Override
                public void cleanOutAllNonFreshMinidumpFiles() {}
            };
        }

        @Override
        public MinidumpUploadCallable createMinidumpUploadCallable(
                File minidumpFile, File logfile) {
            return new MinidumpUploadCallable(minidumpFile, logfile,
                    new MinidumpUploadCallableTest.TestHttpURLConnectionFactory(),
                    mPermissionManager);
        }
    }

    /**
     * Test to ensure the minidump uploading mechanism behaves as expected when we fail to upload
     * minidumps.
     */
    @MediumTest
    public void testFailUploadingMinidumps() throws IOException {
        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsInSample = true;
                        mIsPermitted = true;
                        mIsUserPermitted = true;
                        mIsCommandLineDisabled = false;
                        mIsNetworkAvailable = false; // Will cause us to fail uploads
                        mIsEnabledForTests = false;
                    }
                };
        MinidumpUploaderDelegate delegate = new TestMinidumpUploaderDelegate(
                getInstrumentation().getTargetContext(), permManager);
        MinidumpUploader minidumpUploader = new TestMinidumpUploaderImpl(delegate, permManager);

        File firstFile = createMinidumpFileInCrashDir("1_abc.dmp0");
        File secondFile = createMinidumpFileInCrashDir("12_abc.dmp0");
        String triesBelowMaxString = ".try" + (MinidumpUploaderImpl.MAX_UPLOAD_TRIES_ALLOWED - 1);
        String maxTriesString = ".try" + MinidumpUploaderImpl.MAX_UPLOAD_TRIES_ALLOWED;
        File justBelowMaxTriesFile =
                createMinidumpFileInCrashDir("belowmaxtries.dmp0" + triesBelowMaxString);
        File maxTriesFile = createMinidumpFileInCrashDir("maxtries.dmp0" + maxTriesString);

        File expectedFirstFile = new File(mCrashDir, firstFile.getName() + ".try1");
        File expectedSecondFile = new File(mCrashDir, secondFile.getName() + ".try1");
        File expectedJustBelowMaxTriesFile = new File(mCrashDir,
                justBelowMaxTriesFile.getName().replace(triesBelowMaxString, maxTriesString));

        uploadMinidumpsSync(minidumpUploader, true /* expectReschedule */);
        assertFalse(firstFile.exists());
        assertFalse(secondFile.exists());
        assertFalse(justBelowMaxTriesFile.exists());
        assertTrue(expectedFirstFile.exists());
        assertTrue(expectedSecondFile.exists());
        assertTrue(expectedJustBelowMaxTriesFile.exists());
        // This file should have been left untouched.
        assertTrue(maxTriesFile.exists());
    }

    /**
     * Utility method for running {@param minidumpUploader}.uploadAllMinidumps on the UI thread to
     * avoid breaking any assertions about running on the UI thread.
     */
    private static void uploadAllMinidumpsOnUiThread(final MinidumpUploader minidumpUploader,
            final MinidumpUploader.UploadsFinishedCallback uploadsFinishedCallback) {
        uploadAllMinidumpsOnUiThread(
                minidumpUploader, uploadsFinishedCallback, false /* blockUntilJobPosted */);
    }

    private static void uploadAllMinidumpsOnUiThread(final MinidumpUploader minidumpUploader,
            final MinidumpUploader.UploadsFinishedCallback uploadsFinishedCallback,
            boolean blockUntilJobPosted) {
        final CountDownLatch jobPostedLatch = new CountDownLatch(1);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                minidumpUploader.uploadAllMinidumps(uploadsFinishedCallback);
                jobPostedLatch.countDown();
            }
        });
        if (blockUntilJobPosted) {
            try {
                jobPostedLatch.await();
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }
    }

    private static void uploadMinidumpsSync(
            MinidumpUploader minidumpUploader, final boolean expectReschedule) {
        final CountDownLatch uploadsFinishedLatch = new CountDownLatch(1);
        uploadAllMinidumpsOnUiThread(
                minidumpUploader, new MinidumpUploader.UploadsFinishedCallback() {
                    @Override
                    public void uploadsFinished(boolean reschedule) {
                        assertEquals(expectReschedule, reschedule);
                        uploadsFinishedLatch.countDown();
                    }
                });
        try {
            assertTrue(uploadsFinishedLatch.await(
                    scaleTimeout(TIME_OUT_MILLIS), TimeUnit.MILLISECONDS));
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Ensure MinidumpUploaderImpl doesn't crash even if the WebView Crash dir doesn't exist (could
     * happen e.g. if a Job persists across WebView-updates?
     */
    @MediumTest
    public void testUploadingWithoutCrashDir() throws IOException {
        File webviewCrashDir = getExistingCacheDir();
        // Delete the WebView crash directory to ensure MinidumpUploader doesn't crash without it.
        FileUtils.recursivelyDeleteFile(webviewCrashDir);
        assertFalse(webviewCrashDir.exists());

        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    { mIsEnabledForTests = true; }
                };
        MinidumpUploaderDelegate delegate = new TestMinidumpUploaderDelegate(
                getInstrumentation().getTargetContext(), permManager);
        MinidumpUploader minidumpUploader = new TestMinidumpUploaderImpl(delegate, permManager);

        File firstFile = createMinidumpFileInCrashDir("1_abc.dmp0");
        File secondFile = createMinidumpFileInCrashDir("12_abcd.dmp0");
        File expectedFirstUploadFile =
                new File(mCrashDir, firstFile.getName().replace(".dmp", ".up"));
        File expectedSecondUploadFile =
                new File(mCrashDir, secondFile.getName().replace(".dmp", ".up"));

        uploadMinidumpsSync(minidumpUploader, false /* expectReschedule */);

        assertFalse(firstFile.exists());
        assertTrue(expectedFirstUploadFile.exists());
        assertFalse(secondFile.exists());
        assertTrue(expectedSecondUploadFile.exists());
    }

    private interface MinidumpUploadCallableCreator {
        MinidumpUploadCallable createCallable(File minidumpFile, File logfile);
    }

    private static MinidumpUploaderImpl createCallableListMinidumpUploader(Context context,
            final List<MinidumpUploadCallableCreator> callables, final boolean userPermitted) {
        MinidumpUploaderDelegate delegate = new AwMinidumpUploaderDelegate(context) {
            @Override
            public PlatformServiceBridge createPlatformServiceBridge() {
                return new TestPlatformServiceBridge(true /* canUseGms*/, userPermitted);
            }
        };
        return new TestMinidumpUploaderImpl(delegate, null) {
            private int mIndex = 0;

            @Override
            public MinidumpUploadCallable createMinidumpUploadCallable(
                    File minidumpFile, File logfile) {
                if (mIndex >= callables.size()) {
                    fail("Should not create callable number " + mIndex);
                }
                return callables.get(mIndex++).createCallable(minidumpFile, logfile);
            }
        };
    }

    @MediumTest
    public void testFailingThenPassingUpload() throws IOException {
        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    { mIsEnabledForTests = true; }
                };
        List<MinidumpUploadCallableCreator> callables = new ArrayList();
        callables.add(new MinidumpUploadCallableCreator() {
            @Override
            public MinidumpUploadCallable createCallable(File minidumpFile, File logfile) {
                return new MinidumpUploadCallable(
                        minidumpFile, logfile, new FailingHttpUrlConnectionFactory(), permManager);
            }
        });
        callables.add(new MinidumpUploadCallableCreator() {
            @Override
            public MinidumpUploadCallable createCallable(File minidumpFile, File logfile) {
                return new MinidumpUploadCallable(minidumpFile, logfile,
                        new MinidumpUploadCallableTest.TestHttpURLConnectionFactory(), permManager);
            }
        });
        MinidumpUploader minidumpUploader =
                createCallableListMinidumpUploader(getInstrumentation().getTargetContext(),
                        callables, permManager.isUsageAndCrashReportingPermittedByUser());

        File firstFile = createMinidumpFileInCrashDir("firstFile.dmp0");
        File secondFile = createMinidumpFileInCrashDir("secondFile.dmp0");

        uploadMinidumpsSync(minidumpUploader, true /* expectReschedule */);
        assertFalse(firstFile.exists());
        assertFalse(secondFile.exists());
        File expectedSecondFile = null;
        // Not sure which minidump will fail and which will succeed, so just ensure one was uploaded
        // and the other one failed.
        if (new File(mCrashDir, firstFile.getName() + ".try1").exists()) {
            expectedSecondFile = new File(mCrashDir, secondFile.getName().replace(".dmp", ".up"));
        } else {
            File uploadedFirstFile =
                    new File(mCrashDir, firstFile.getName().replace(".dmp", ".up"));
            assertTrue(uploadedFirstFile.exists());
            expectedSecondFile = new File(mCrashDir, secondFile.getName() + ".try1");
        }
        assertTrue(expectedSecondFile.exists());
    }

    private static class StallingHttpUrlConnectionFactory implements HttpURLConnectionFactory {
        private final CountDownLatch mStopStallingLatch;
        private final boolean mSucceed;

        private class StallingOutputStream extends OutputStream {
            @Override
            public void write(int b) throws IOException {
                try {
                    mStopStallingLatch.await();
                } catch (InterruptedException e) {
                    throw new InterruptedIOException(e.toString());
                }
                if (!mSucceed) {
                    throw new IOException();
                }
            }
        }

        public StallingHttpUrlConnectionFactory(CountDownLatch stopStallingLatch, boolean succeed) {
            mStopStallingLatch = stopStallingLatch;
            mSucceed = succeed;
        }

        public HttpURLConnection createHttpURLConnection(String url) {
            try {
                return new MinidumpUploadCallableTest.TestHttpURLConnection(new URL(url)) {
                    @Override
                    public OutputStream getOutputStream() {
                        return new StallingOutputStream();
                    }
                };
            } catch (MalformedURLException e) {
                return null;
            }
        }
    }

    private static class FailingHttpUrlConnectionFactory implements HttpURLConnectionFactory {
        public HttpURLConnection createHttpURLConnection(String url) {
            return null;
        }
    }

    /**
     * Test that ensures we can interrupt the MinidumpUploader when uploading minidumps.
     */
    @MediumTest
    public void testCancelMinidumpUploadsFailedUpload() throws IOException {
        testCancellation(false /* successfulUpload */);
    }

    /**
     * Test that ensures interrupting our upload-job will not interrupt the first upload.
     */
    @MediumTest
    public void testCancelingWontCancelFirstUpload() throws IOException {
        testCancellation(true /* successfulUpload */);
    }

    private void testCancellation(final boolean successfulUpload) throws IOException {
        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    { mIsEnabledForTests = true; }
                };
        final CountDownLatch stopStallingLatch = new CountDownLatch(1);
        MinidumpUploaderDelegate delegate = new TestMinidumpUploaderDelegate(
                getInstrumentation().getTargetContext(), permManager) {
            @Override
            public PlatformServiceBridge createPlatformServiceBridge() {
                return new TestPlatformServiceBridge(
                        true /* canUseGms*/, permManager.isUsageAndCrashReportingPermittedByUser());
            }
        };
        MinidumpUploaderImpl minidumpUploader = new TestMinidumpUploaderImpl(
                delegate, permManager) {
            @Override
            public MinidumpUploadCallable createMinidumpUploadCallable(
                    File minidumpFile, File logfile) {
                return new MinidumpUploadCallable(minidumpFile, logfile,
                        new StallingHttpUrlConnectionFactory(stopStallingLatch, successfulUpload),
                        permManager);
            }
        };

        File firstFile = createMinidumpFileInCrashDir("123_abc.dmp0");
        File expectedFirstUploadFile =
                new File(mCrashDir, firstFile.getName().replace(".dmp", ".up"));
        File expectedFirstRetryFile = new File(mCrashDir, firstFile.getName() + ".try1");

        // This is run on the UI thread to avoid failing any assertOnUiThread assertions.
        uploadAllMinidumpsOnUiThread(minidumpUploader,
                new MinidumpUploader.UploadsFinishedCallback() {
                    @Override
                    public void uploadsFinished(boolean reschedule) {
                        if (successfulUpload) {
                            assertFalse(reschedule);
                        } else {
                            fail("This method shouldn't be called when a canceled upload fails.");
                        }
                    }
                },
                // Block until job posted - otherwise the worker thread might not have been created
                // before we try to join it.
                true /* blockUntilJobPosted */);
        minidumpUploader.cancelUploads();
        stopStallingLatch.countDown();
        // Wait until our job finished.
        try {
            minidumpUploader.joinWorkerThreadForTesting();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }

        if (successfulUpload) {
            // When the upload succeeds we expect the file to be renamed.
            assertFalse(firstFile.exists());
            assertTrue(expectedFirstUploadFile.exists());
            assertFalse(expectedFirstRetryFile.exists());
        } else {
            // When the upload fails we won't change the minidump at all.
            assertTrue(firstFile.exists());
            assertFalse(expectedFirstUploadFile.exists());
            assertFalse(expectedFirstRetryFile.exists());
        }
    }

    /**
     * Ensures that the minidump copying works together with the minidump uploading.
     */
    @MediumTest
    public void testCopyAndUploadWebViewMinidump() throws FileNotFoundException, IOException {
        final CrashFileManager fileManager = new CrashFileManager(
                CrashReceiverService.getWebViewCrashDir(getInstrumentation().getTargetContext()));
        // Note that these minidump files are set up directly in the cache dir - not in the WebView
        // crash dir. This is to ensure the CrashFileManager doesn't see these minidumps without us
        // first copying them.
        File minidumpToCopy =
                new File(getInstrumentation().getTargetContext().getCacheDir(), "toCopy.dmp");
        setUpMinidumpFile(minidumpToCopy, BOUNDARY, "browser");
        final String expectedFileContent = readEntireFile(minidumpToCopy);

        File[] uploadedFiles = copyAndUploadMinidumpsSync(
                fileManager, new File[][] {{minidumpToCopy}}, new int[] {0});

        // CrashReceiverService will rename the minidumps to some globally unique file name
        // meaning that we have to check the contents of the minidump rather than the file
        // name.
        try {
            assertEquals(expectedFileContent, readEntireFile(uploadedFiles[0]));
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
        File webviewTmpDir =
                CrashReceiverService.getWebViewTmpCrashDir(getInstrumentation().getTargetContext());
        assertEquals(0, webviewTmpDir.listFiles().length);
    }

    /**
     * Ensure that when PlatformServiceBridge returns true we do upload minidumps.
     */
    @MediumTest
    public void testPlatformServicesBridgeIsUsedUserConsent() throws IOException {
        testPlatformServicesBridgeIsUsed(true);
    }

    /**
     * Ensure that when PlatformServiceBridge returns false we do not upload minidumps.
     */
    @MediumTest
    public void testPlatformServicesBridgeIsUsedNoUserConsent() throws IOException {
        testPlatformServicesBridgeIsUsed(false);
    }

    /**
     * MinidumpUploaderDelegate sub-class that uses MinidumpUploaderDelegate's implementation of
     * CrashReportingPermissionManager.isUsageAndCrashReportingPermittedByUser().
     */
    private static class WebViewUserConsentMinidumpUploaderDelegate
            extends AwMinidumpUploaderDelegate {
        private final boolean mUserConsent;
        WebViewUserConsentMinidumpUploaderDelegate(Context context, boolean userConsent) {
            super(context);
            mUserConsent = userConsent;
        }
        @Override
        public PlatformServiceBridge createPlatformServiceBridge() {
            return new TestPlatformServiceBridge(true /* canUseGms */, mUserConsent);
        }
        @Override
        public CrashReportingPermissionManager createCrashReportingPermissionManager() {
            final CrashReportingPermissionManager realPermissionManager =
                    super.createCrashReportingPermissionManager();
            return new MockCrashReportingPermissionManager() {
                {
                    // This setup ensures we depend on
                    // isUsageAndCrashReportingPermittedByUser().
                    mIsInSample = true;
                    mIsPermitted = true;
                    mIsCommandLineDisabled = false;
                    mIsNetworkAvailable = true;
                    mIsEnabledForTests = false;
                }
                @Override
                public boolean isUsageAndCrashReportingPermittedByUser() {
                    // Ensure that we use the real implementation of
                    // isUsageAndCrashReportingPermittedByUser.
                    boolean userPermitted =
                            realPermissionManager.isUsageAndCrashReportingPermittedByUser();
                    assertEquals(mUserConsent, userPermitted);
                    return userPermitted;
                }
            };
        }
    }

    private void testPlatformServicesBridgeIsUsed(final boolean userConsent) throws IOException {
        MinidumpUploaderDelegate delegate = new WebViewUserConsentMinidumpUploaderDelegate(
                getInstrumentation().getTargetContext(), userConsent);
        MinidumpUploader minidumpUploader = new TestMinidumpUploaderImpl(
                delegate, delegate.createCrashReportingPermissionManager());

        File firstFile = createMinidumpFileInCrashDir("1_abc.dmp0");
        File secondFile = createMinidumpFileInCrashDir("12_abcd.dmp0");
        File expectedFirstFile = new File(
                mCrashDir, firstFile.getName().replace(".dmp", userConsent ? ".up" : ".skipped"));
        File expectedSecondFile = new File(
                mCrashDir, secondFile.getName().replace(".dmp", userConsent ? ".up" : ".skipped"));

        uploadMinidumpsSync(minidumpUploader, false /* expectReschedule */);

        assertFalse(firstFile.exists());
        assertTrue(expectedFirstFile.exists());
        assertFalse(secondFile.exists());
        assertTrue(expectedSecondFile.exists());
    }

    private static String readEntireFile(File file) throws IOException {
        FileInputStream fileInputStream = new FileInputStream(file);
        try {
            byte[] data = new byte[(int) file.length()];
            fileInputStream.read(data);
            return new String(data);
        } finally {
            fileInputStream.close();
        }
    }

    /**
     * Ensure that the minidump copying doesn't trigger when we pass it invalid file descriptors.
     */
    @MediumTest
    public void testCopyingAbortsForInvalidFds() throws FileNotFoundException, IOException {
        assertFalse(CrashReceiverService.copyMinidumps(
                getInstrumentation().getTargetContext(), 0 /* uid */, null));
        assertFalse(CrashReceiverService.copyMinidumps(getInstrumentation().getTargetContext(),
                0 /* uid */, new ParcelFileDescriptor[] {null, null}));
        assertFalse(CrashReceiverService.copyMinidumps(
                getInstrumentation().getTargetContext(), 0 /* uid */, new ParcelFileDescriptor[0]));
    }

    /**
     * Ensure deleting temporary files used when copying minidumps works correctly.
     */
    @MediumTest
    public void testDeleteFilesInDir() throws IOException {
        Context context = getInstrumentation().getTargetContext();
        File webviewTmpDir = CrashReceiverService.getWebViewTmpCrashDir(context);
        if (!webviewTmpDir.isDirectory()) {
            assertTrue(webviewTmpDir.mkdir());
        }
        File testFile1 = new File(webviewTmpDir, "testFile1");
        File testFile2 = new File(webviewTmpDir, "testFile2");
        assertTrue(testFile1.createNewFile());
        assertTrue(testFile2.createNewFile());
        assertTrue(testFile1.exists());
        assertTrue(testFile2.exists());
        CrashReceiverService.deleteFilesInWebViewTmpDirIfExists(context);
        assertFalse(testFile1.exists());
        assertFalse(testFile2.exists());
    }

    /**
     * Ensure we can copy and upload several batches of files (i.e. emulate several copying-calls in
     * a row without the copying-service being destroyed in between).
     */
    @MediumTest
    public void testCopyAndUploadSeveralMinidumpBatches() throws IOException {
        final CrashFileManager fileManager = new CrashFileManager(
                CrashReceiverService.getWebViewCrashDir(getInstrumentation().getTargetContext()));
        // Note that these minidump files are set up directly in the cache dir - not in the WebView
        // crash dir. This is to ensure the CrashFileManager doesn't see these minidumps without us
        // first copying them.
        File firstMinidumpToCopy =
                new File(getInstrumentation().getTargetContext().getCacheDir(), "firstToCopy.dmp");
        File secondMinidumpToCopy =
                new File(getInstrumentation().getTargetContext().getCacheDir(), "secondToCopy.dmp");
        setUpMinidumpFile(firstMinidumpToCopy, BOUNDARY, "browser");
        setUpMinidumpFile(secondMinidumpToCopy, BOUNDARY, "renderer");
        final String expectedFirstFileContent = readEntireFile(firstMinidumpToCopy);
        final String expectedSecondFileContent = readEntireFile(secondMinidumpToCopy);

        File[] uploadedFiles = copyAndUploadMinidumpsSync(fileManager,
                new File[][] {{firstMinidumpToCopy}, {secondMinidumpToCopy}}, new int[] {0, 0});

        // CrashReceiverService will rename the minidumps to some globally unique file name
        // meaning that we have to check the contents of the minidump rather than the file
        // name.
        try {
            final String actualFileContent0 = readEntireFile(uploadedFiles[0]);
            final String actualFileContent1 = readEntireFile(uploadedFiles[1]);
            if (expectedFirstFileContent.equals(actualFileContent0)) {
                assertEquals(expectedSecondFileContent, actualFileContent1);
            } else {
                assertEquals(expectedFirstFileContent, actualFileContent1);
                assertEquals(expectedSecondFileContent, actualFileContent0);
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Copy and upload {@param minidumps} by one array at a time - i.e. the minidumps in a single
     * array in {@param minidumps} will all be copied in the same call into CrashReceiverService.
     * @param fileManager the CrashFileManager to use when copying/renaming minidumps.
     * @param minidumps an array of arrays of minidumps to copy and upload, by copying one array at
     * a time.
     * @param uids an array of uids declaring the uids used when calling into CrashReceiverService.
     * @return the uploaded files.
     */
    private File[] copyAndUploadMinidumpsSync(CrashFileManager fileManager, File[][] minidumps,
            int[] uids) throws FileNotFoundException {
        CrashReceiverService crashReceiverService = new CrashReceiverService();
        assertEquals(minidumps.length, uids.length);
        // Ensure the upload service minidump directory is empty before we start copying files.
        File[] initialMinidumps =
                fileManager.getAllMinidumpFiles(MinidumpUploaderImpl.MAX_UPLOAD_TRIES_ALLOWED);
        assertEquals(0, initialMinidumps.length);

        // Open file descriptors to the files and then delete the files.
        ParcelFileDescriptor[][] fileDescriptors = new ParcelFileDescriptor[minidumps.length][];
        int numMinidumps = 0;
        for (int n = 0; n < minidumps.length; n++) {
            File[] currentMinidumps = minidumps[n];
            numMinidumps += currentMinidumps.length;
            fileDescriptors[n] = new ParcelFileDescriptor[currentMinidumps.length];
            for (int m = 0; m < currentMinidumps.length; m++) {
                fileDescriptors[n][m] = ParcelFileDescriptor.open(
                        currentMinidumps[m], ParcelFileDescriptor.MODE_READ_ONLY);
                assertTrue(currentMinidumps[m].delete());
            }
            crashReceiverService.performMinidumpCopyingSerially(
                    getInstrumentation().getTargetContext(), uids[n] /* uid */, fileDescriptors[n],
                    false /* scheduleUploads */);
        }

        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    { mIsEnabledForTests = true; }
                };
        MinidumpUploaderDelegate delegate = new TestMinidumpUploaderDelegate(
                getInstrumentation().getTargetContext(), permManager);
        MinidumpUploader minidumpUploader = new TestMinidumpUploaderImpl(delegate, permManager);

        uploadMinidumpsSync(minidumpUploader, false /* expectReschedule */);
        // Ensure there are no minidumps left to upload.
        File[] nonUploadedMinidumps =
                fileManager.getAllMinidumpFiles(MinidumpUploaderImpl.MAX_UPLOAD_TRIES_ALLOWED);
        assertEquals(0, nonUploadedMinidumps.length);

        File[] uploadedFiles = fileManager.getAllUploadedFiles();
        assertEquals(numMinidumps, uploadedFiles.length);
        return uploadedFiles;
    }

    private File createMinidumpFileInCrashDir(String name) throws IOException {
        File minidumpFile = new File(mCrashDir, name);
        setUpMinidumpFile(minidumpFile, BOUNDARY);
        return minidumpFile;
    }
}
