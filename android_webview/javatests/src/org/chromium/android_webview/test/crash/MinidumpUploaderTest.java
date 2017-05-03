// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.crash;

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
import org.chromium.components.minidump_uploader.MinidumpUploadTestUtility;
import org.chromium.components.minidump_uploader.MinidumpUploader;
import org.chromium.components.minidump_uploader.MinidumpUploaderDelegate;
import org.chromium.components.minidump_uploader.MinidumpUploaderImpl;
import org.chromium.components.minidump_uploader.TestMinidumpUploaderImpl;
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

/**
 * Instrumentation tests for MinidumpUploader.
 */
public class MinidumpUploaderTest extends CrashTestCase {
    private static final String TAG = "MinidumpUploaderTest";
    private static final String BOUNDARY = "TESTBOUNDARY";

    @Override
    protected File getExistingCacheDir() {
        return CrashReceiverService.getOrCreateWebViewCrashDir();
    }

    private static class TestPlatformServiceBridge extends PlatformServiceBridge {
        private final boolean mEnabled;

        public TestPlatformServiceBridge(boolean enabled) {
            mEnabled = enabled;
        }

        @Override
        public boolean canUseGms() {
            return true;
        }

        @Override
        public void queryMetricsSetting(ValueCallback<Boolean> callback) {
            ThreadUtils.assertOnUiThread();
            callback.onReceiveValue(mEnabled);
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
                        mIsUserPermitted = true;
                        mIsNetworkAvailable = false; // Will cause us to fail uploads
                        mIsEnabledForTests = false;
                    }
                };
        MinidumpUploader minidumpUploader =
                new TestMinidumpUploaderImpl(getExistingCacheDir(), permManager);

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

        MinidumpUploadTestUtility.uploadMinidumpsSync(
                minidumpUploader, true /* expectReschedule */);
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
     * Ensure MinidumpUploaderImpl doesn't crash even if the WebView Crash dir doesn't exist (could
     * happen e.g. if a Job persists across WebView-updates?
     *
     * MinidumpUploaderImpl should automatically recreate the directory.
     */
    @MediumTest
    public void testUploadingWithoutCrashDir() throws IOException {
        File webviewCrashDir = getExistingCacheDir();
        // Delete the WebView crash directory to ensure MinidumpUploader doesn't crash without it.
        FileUtils.recursivelyDeleteFile(webviewCrashDir);
        assertFalse(webviewCrashDir.exists());

        PlatformServiceBridge.injectInstance(new TestPlatformServiceBridge(true));
        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    { mIsEnabledForTests = true; }
                };
        MinidumpUploader minidumpUploader =
                // Use AwMinidumpUploaderDelegate instead of TestMinidumpUploaderDelegate here
                // since AwMinidumpUploaderDelegate defines the WebView crash directory.
                new TestMinidumpUploaderImpl(new AwMinidumpUploaderDelegate() {
                    @Override
                    public CrashReportingPermissionManager createCrashReportingPermissionManager() {
                        return permManager;
                    }
                });

        // Ensure that we don't crash when trying to upload minidumps without a crash directory.
        MinidumpUploadTestUtility.uploadMinidumpsSync(
                minidumpUploader, false /* expectReschedule */);
    }

    private interface MinidumpUploadCallableCreator {
        MinidumpUploadCallable createCallable(File minidumpFile, File logfile);
    }

    private MinidumpUploaderImpl createCallableListMinidumpUploader(
            final List<MinidumpUploadCallableCreator> callables, final boolean userPermitted) {
        return new TestMinidumpUploaderImpl(getExistingCacheDir(), null) {
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
        List<MinidumpUploadCallableCreator> callables = new ArrayList<>();
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
        MinidumpUploader minidumpUploader = createCallableListMinidumpUploader(
                callables, permManager.isUsageAndCrashReportingPermittedByUser());

        File firstFile = createMinidumpFileInCrashDir("firstFile.dmp0");
        File secondFile = createMinidumpFileInCrashDir("secondFile.dmp0");

        MinidumpUploadTestUtility.uploadMinidumpsSync(
                minidumpUploader, true /* expectReschedule */);
        assertFalse(firstFile.exists());
        assertFalse(secondFile.exists());
        File expectedSecondFile;
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

    /**
     * Minidump uploader implementation that stalls minidump-uploading until a given CountDownLatch
     * counts down.
     */
    private static class StallingMinidumpUploaderImpl extends TestMinidumpUploaderImpl {
        CountDownLatch mStopStallingLatch;
        boolean mSuccessfulUpload;

        public StallingMinidumpUploaderImpl(File cacheDir,
                CrashReportingPermissionManager permissionManager, CountDownLatch stopStallingLatch,
                boolean successfulUpload) {
            super(cacheDir, permissionManager);
            mStopStallingLatch = stopStallingLatch;
            mSuccessfulUpload = successfulUpload;
        }

        @Override
        public MinidumpUploadCallable createMinidumpUploadCallable(
                File minidumpFile, File logfile) {
            return new MinidumpUploadCallable(minidumpFile, logfile,
                    new StallingHttpUrlConnectionFactory(mStopStallingLatch, mSuccessfulUpload),
                    mDelegate.createCrashReportingPermissionManager());
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
        MinidumpUploaderImpl minidumpUploader = new StallingMinidumpUploaderImpl(
                getExistingCacheDir(), permManager, stopStallingLatch, successfulUpload);

        File firstFile = createMinidumpFileInCrashDir("123_abc.dmp0");
        File expectedFirstUploadFile =
                new File(mCrashDir, firstFile.getName().replace(".dmp", ".up"));
        File expectedFirstRetryFile = new File(mCrashDir, firstFile.getName() + ".try1");

        // This is run on the UI thread to avoid failing any assertOnUiThread assertions.
        MinidumpUploadTestUtility.uploadAllMinidumpsOnUiThread(minidumpUploader,
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
     * Ensure that canceling an upload that fails causes a reschedule.
     */
    @MediumTest
    public void testCancelFailedUploadCausesReschedule() throws IOException {
        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    { mIsEnabledForTests = true; }
                };
        final CountDownLatch stopStallingLatch = new CountDownLatch(1);
        MinidumpUploaderImpl minidumpUploader =
                new StallingMinidumpUploaderImpl(getExistingCacheDir(), permManager,
                        stopStallingLatch, false /* successfulUpload */);

        createMinidumpFileInCrashDir("123_abc.dmp0");

        MinidumpUploader.UploadsFinishedCallback crashingCallback =
                new MinidumpUploader.UploadsFinishedCallback() {
                    @Override
                    public void uploadsFinished(boolean reschedule) {
                        // We don't guarantee whether uploadsFinished is called after a job has been
                        // cancelled, but if it is, it should indicate that we want to reschedule
                        // the job.
                        assertTrue(reschedule);
                    }
                };

        // This is run on the UI thread to avoid failing any assertOnUiThread assertions.
        MinidumpUploadTestUtility.uploadAllMinidumpsOnUiThread(minidumpUploader, crashingCallback);
        // Ensure we tell JobScheduler to reschedule the job.
        assertTrue(minidumpUploader.cancelUploads());
        stopStallingLatch.countDown();
    }

    /**
     * Ensures that the minidump copying works together with the minidump uploading.
     */
    @MediumTest
    public void testCopyAndUploadWebViewMinidump() throws IOException {
        final CrashFileManager fileManager =
                new CrashFileManager(CrashReceiverService.getWebViewCrashDir());
        // Note that these minidump files are set up directly in the cache dir - not in the WebView
        // crash dir. This is to ensure the CrashFileManager doesn't see these minidumps without us
        // first copying them.
        File minidumpToCopy = new File(getExistingCacheDir(), "toCopy.dmp");
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
        File webviewTmpDir = CrashReceiverService.getWebViewTmpCrashDir();
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
        WebViewUserConsentMinidumpUploaderDelegate(boolean userConsent) {
            super();
            mUserConsent = userConsent;
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
        PlatformServiceBridge.injectInstance(new TestPlatformServiceBridge(userConsent));
        MinidumpUploaderDelegate delegate =
                new WebViewUserConsentMinidumpUploaderDelegate(userConsent);
        MinidumpUploader minidumpUploader = new TestMinidumpUploaderImpl(delegate);

        File firstFile = createMinidumpFileInCrashDir("1_abc.dmp0");
        File secondFile = createMinidumpFileInCrashDir("12_abcd.dmp0");
        File expectedFirstFile = new File(
                mCrashDir, firstFile.getName().replace(".dmp", userConsent ? ".up" : ".skipped"));
        File expectedSecondFile = new File(
                mCrashDir, secondFile.getName().replace(".dmp", userConsent ? ".up" : ".skipped"));

        MinidumpUploadTestUtility.uploadMinidumpsSync(
                minidumpUploader, false /* expectReschedule */);

        assertFalse(firstFile.exists());
        assertTrue(expectedFirstFile.exists());
        assertFalse(secondFile.exists());
        assertTrue(expectedSecondFile.exists());
    }

    private static String readEntireFile(File file) throws IOException {
        try (FileInputStream fileInputStream = new FileInputStream(file)) {
            byte[] data = new byte[(int) file.length()];
            fileInputStream.read(data);
            return new String(data);
        }
    }

    /**
     * Ensure we can copy and upload several batches of files (i.e. emulate several copying-calls in
     * a row without the copying-service being destroyed in between).
     */
    @MediumTest
    public void testCopyAndUploadSeveralMinidumpBatches() throws IOException {
        final CrashFileManager fileManager =
                new CrashFileManager(CrashReceiverService.getWebViewCrashDir());
        // Note that these minidump files are set up directly in the cache dir - not in the WebView
        // crash dir. This is to ensure the CrashFileManager doesn't see these minidumps without us
        // first copying them.
        File firstMinidumpToCopy = new File(getExistingCacheDir(), "firstToCopy.dmp");
        File secondMinidumpToCopy = new File(getExistingCacheDir(), "secondToCopy.dmp");
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
                    uids[n] /* uid */, fileDescriptors[n], false /* scheduleUploads */);
        }

        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    { mIsEnabledForTests = true; }
                };
        MinidumpUploader minidumpUploader =
                // Use AwMinidumpUploaderDelegate instead of TestMinidumpUploaderDelegate to ensure
                // AwMinidumpUploaderDelegate works well together with the minidump-copying methods
                // of CrashReceiverService.
                new TestMinidumpUploaderImpl(new AwMinidumpUploaderDelegate() {
                    @Override
                    public CrashReportingPermissionManager createCrashReportingPermissionManager() {
                        return permManager;
                    }
                });

        MinidumpUploadTestUtility.uploadMinidumpsSync(
                minidumpUploader, false /* expectReschedule */);
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
