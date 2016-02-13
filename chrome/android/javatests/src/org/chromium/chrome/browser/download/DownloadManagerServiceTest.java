// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.DownloadManager;
import android.content.Context;
import android.content.Intent;
import android.os.Environment;
import android.os.Handler;
import android.os.HandlerThread;
import android.preference.PreferenceManager;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;
import android.util.Pair;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.download.DownloadManagerServiceTest.MockDownloadNotifier.MethodID;
import org.chromium.content.browser.DownloadInfo;
import org.chromium.content.browser.DownloadInfo.Builder;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.Collections;
import java.util.HashSet;
import java.util.Queue;
import java.util.Random;
import java.util.Set;
import java.util.concurrent.ConcurrentLinkedQueue;

/**
 * Test for DownloadManagerService.
 */
public class DownloadManagerServiceTest extends InstrumentationTestCase {
    private static final int UPDATE_DELAY_FOR_TEST = 1;
    private static final int DELAY_BETWEEN_CALLS = 10;
    private static final int LONG_UPDATE_DELAY_FOR_TEST = 500;
    private static final String INSTALL_NOTIFY_URI = "http://test/test";
    private final Random mRandom = new Random();

    /**
     * The MockDownloadNotifier. Currently there is no support for creating mock objects this is a
     * simple mock object that provides testing support for checking a sequence of calls.
     */
    static class MockDownloadNotifier
            implements org.chromium.chrome.browser.download.DownloadNotifier {
        /**
         * The Ids of different methods in this mock object.
         */
        static enum MethodID {
            DOWNLOAD_SUCCESSFUL,
            DOWNLOAD_FAILED,
            DOWNLOAD_PROGRESS,
            CANCEL_DOWNLOAD_ID
        }

        private final Queue<Pair<MethodID, Object>> mExpectedCalls =
                new ConcurrentLinkedQueue<Pair<MethodID, Object>>();

        public MockDownloadNotifier expect(MethodID method, Object param) {
            mExpectedCalls.clear();
            mExpectedCalls.add(getMethodSignature(method, param));
            return this;
        }

        public void waitTillExpectedCallsComplete() {
            try {
                CriteriaHelper.pollForCriteria(
                        new Criteria("Failed while waiting for all calls to complete.") {
                            @Override
                            public boolean isSatisfied() {
                                return mExpectedCalls.isEmpty();
                            }
                        });
            } catch (InterruptedException e) {
                fail("Failed while waiting for all calls to complete." + e);
            }
        }

        public MockDownloadNotifier andThen(MethodID method, Object param) {
            mExpectedCalls.add(getMethodSignature(method, param));
            return this;
        }

        static Pair<MethodID, Object> getMethodSignature(MethodID methodId, Object param) {
            return new Pair<MethodID, Object>(methodId, param);
        }

        void assertCorrectExpectedCall(MethodID methodId, Object param) {
            Log.w("MockDownloadNotifier", "Called: " + methodId);
            assertFalse("Unexpected call:, no call expected, but got: " + methodId,
                    mExpectedCalls.isEmpty());
            Pair<MethodID, Object> actual = getMethodSignature(methodId, param);
            Pair<MethodID, Object> expected = mExpectedCalls.poll();
            assertEquals("Unexpected call", expected.first, actual.first);
            assertTrue("Incorrect arguments", MatchHelper.macthes(expected.second, actual.second));
        }

        @Override
        public void notifyDownloadSuccessful(DownloadInfo downloadInfo, Intent intent) {
            assertCorrectExpectedCall(MethodID.DOWNLOAD_SUCCESSFUL, downloadInfo);
        }

        @Override
        public void notifyDownloadFailed(DownloadInfo downloadInfo) {
            assertCorrectExpectedCall(MethodID.DOWNLOAD_FAILED, downloadInfo);

        }

        @Override
        public void notifyDownloadProgress(DownloadInfo downloadInfo, long startTime) {
            assertCorrectExpectedCall(MethodID.DOWNLOAD_PROGRESS, downloadInfo);
        }

        @Override
        public void cancelNotification(int downloadId) {
            assertCorrectExpectedCall(MethodID.CANCEL_DOWNLOAD_ID, downloadId);
        }
    }

    /**
     * Mock implementation of the DownloadSnackbarController.
     */
    static class MockDownloadSnackbarController extends DownloadSnackbarController {
        private boolean mSucceeded;
        private boolean mFailed;

        public MockDownloadSnackbarController() {
            super(null);
        }

        public void waitForSnackbarControllerToFinish(final boolean success) {
            try {
                CriteriaHelper.pollForCriteria(
                        new Criteria("Failed while waiting for all calls to complete.") {
                            @Override
                            public boolean isSatisfied() {
                                return success ? mSucceeded : mFailed;
                            }
                        });
            } catch (InterruptedException e) {
                fail("Failed while waiting for all calls to complete." + e);
            }
        }

        @Override
        public void onDownloadSucceeded(
                DownloadInfo downloadInfo, final long downloadId, boolean canBeResolved) {
            mSucceeded = true;
        }

        @Override
        public void onDownloadFailed(String errorMessage, boolean showAllDownloads) {
            mFailed = true;
        }
    }

    /**
     * A set that each object can be matched ^only^ once. Once matched, the object
     * will be removed from the set. This is useful to write expectations
     * for a sequence of calls where order of calls is not defined. Client can
     * do the following. OneTimeMatchSet matchSet = new OneTimeMatchSet(possibleValue1,
     * possibleValue2, possibleValue3); mockObject.expect(method1, matchSet).andThen(method1,
     * matchSet).andThen(method3, matchSet); .... Some work.
     * mockObject.waitTillExpectedCallsComplete(); assertTrue(matchSet.mMatches.empty());
     */
    private static class OneTimeMatchSet {
        private final HashSet<Object> mMatches;

        OneTimeMatchSet(Object... params) {
            mMatches = new HashSet<Object>();
            Collections.addAll(mMatches, params);
        }

        public boolean matches(Object obj) {
            if (obj == null) return false;
            if (this == obj) return true;
            if (!mMatches.contains(obj)) return false;

            // Remove the object since it has been matched.
            mMatches.remove(obj);
            return true;
        }
    }

    /**
     * Class that helps matching 2 objects with either of them may be a OneTimeMatchSet object.
     */
    private static class MatchHelper {
        public static boolean macthes(Object obj1, Object obj2) {
            if (obj1 == null) return obj2 == null;
            if (obj1.equals(obj2)) return true;
            if (obj1 instanceof OneTimeMatchSet) {
                return ((OneTimeMatchSet) obj1).matches(obj2);
            } else if (obj2 instanceof OneTimeMatchSet) {
                return ((OneTimeMatchSet) obj2).matches(obj1);
            }
            return false;
        }
    }

    static class MockOMADownloadHandler extends OMADownloadHandler {
        protected boolean mSuccess;
        protected String mNofityURI;
        protected DownloadInfo mDownloadInfo;
        protected long mDownloadId;

        MockOMADownloadHandler(Context context) {
            super(context);
        }

        protected void setDownloadId(long downloadId) {
            mDownloadId = downloadId;
        }

        @Override
        public void onDownloadCompleted(DownloadInfo downloadInfo, String notifyURI) {
            mSuccess = true;
            mNofityURI = notifyURI;
        }

        @Override
        public boolean isPendingOMADownload(long downloadId) {
            return mDownloadId == downloadId;
        }

        @Override
        public DownloadInfo updateDownloadInfo(DownloadInfo downloadInfo, long newDownloadId) {
            mDownloadInfo = downloadInfo;
            mDownloadId = newDownloadId;
            mDownloadInfo = DownloadInfo.Builder.fromDownloadInfo(downloadInfo)
                      .setDownloadId((int) newDownloadId)
                      .build();
            return mDownloadInfo;
        }

        @Override
        public String getInstallNotifyInfo(long downloadId) {
            return INSTALL_NOTIFY_URI;
        }
    }

    private static class DownloadManagerServiceForTest extends DownloadManagerService {
        public DownloadManagerServiceForTest(Context context, MockDownloadNotifier mockNotifier,
                long updateDelayInMillis) {
            super(context, mockNotifier, getTestHandler(), updateDelayInMillis);
        }

        @Override
        protected long addCompletedDownload(DownloadInfo downloadInfo) {
            return 1L;
        }

        @Override
        protected void init() {}
    }

    private static Handler getTestHandler() {
        HandlerThread handlerThread = new HandlerThread("handlerThread");
        handlerThread.start();
        return new Handler(handlerThread.getLooper());
    }

    private DownloadInfo getDownloadInfo() {
        return new Builder().setContentLength(100)
                .setDownloadId(mRandom.nextInt(1000))
                .setHasDownloadId(true)
                .build();
    }

    private Context getTestContext() {
        return new AdvancedMockContext(getInstrumentation().getTargetContext());
    }

    @MediumTest
    @Feature({"Download"})
    public void testDownloadProgressIsCalled() throws InterruptedException {
        MockDownloadNotifier notifier = new MockDownloadNotifier();
        DownloadManagerServiceForTest dService = new DownloadManagerServiceForTest(
                getTestContext(), notifier, UPDATE_DELAY_FOR_TEST);
        DownloadInfo downloadInfo = getDownloadInfo();

        notifier.expect(MethodID.DOWNLOAD_PROGRESS, downloadInfo);
        dService.onDownloadUpdated(downloadInfo);
        notifier.waitTillExpectedCallsComplete();

        // Now post multiple download updated calls and make sure all are received.
        DownloadInfo update1 = Builder.fromDownloadInfo(downloadInfo)
                .setPercentCompleted(10).build();
        DownloadInfo update2 = Builder.fromDownloadInfo(downloadInfo)
                .setPercentCompleted(30).build();
        DownloadInfo update3 = Builder.fromDownloadInfo(downloadInfo)
                .setPercentCompleted(30).build();
        notifier.expect(MethodID.DOWNLOAD_PROGRESS, update1)
                .andThen(MethodID.DOWNLOAD_PROGRESS, update2)
                .andThen(MethodID.DOWNLOAD_PROGRESS, update3);

        dService.onDownloadUpdated(update1);
        Thread.sleep(DELAY_BETWEEN_CALLS);
        dService.onDownloadUpdated(update2);
        Thread.sleep(DELAY_BETWEEN_CALLS);
        dService.onDownloadUpdated(update3);
        notifier.waitTillExpectedCallsComplete();
    }

    @MediumTest
    @Feature({"Download"})
    public void testOnlyOneProgressForFastUpdates() throws InterruptedException {
        MockDownloadNotifier notifier = new MockDownloadNotifier();
        DownloadManagerServiceForTest dService = new DownloadManagerServiceForTest(
                getTestContext(), notifier, LONG_UPDATE_DELAY_FOR_TEST);
        DownloadInfo downloadInfo = getDownloadInfo();
        DownloadInfo update1 = Builder.fromDownloadInfo(downloadInfo)
                .setPercentCompleted(10).build();
        DownloadInfo update2 = Builder.fromDownloadInfo(downloadInfo)
                .setPercentCompleted(30).build();
        DownloadInfo update3 = Builder.fromDownloadInfo(downloadInfo)
                .setPercentCompleted(30).build();

        // Should only get one update call, the last update.
        notifier.expect(MethodID.DOWNLOAD_PROGRESS, update3);
        dService.onDownloadUpdated(update1);
        Thread.sleep(DELAY_BETWEEN_CALLS);
        dService.onDownloadUpdated(update2);
        Thread.sleep(DELAY_BETWEEN_CALLS);
        dService.onDownloadUpdated(update3);
        Thread.sleep(DELAY_BETWEEN_CALLS);
        notifier.waitTillExpectedCallsComplete();
    }

    @MediumTest
    @Feature({"Download"})
    public void testDownloadCompletedIsCalled() throws InterruptedException {
        MockDownloadNotifier notifier = new MockDownloadNotifier();
        MockDownloadSnackbarController snackbarController = new MockDownloadSnackbarController();
        DownloadManagerServiceForTest dService = new DownloadManagerServiceForTest(
                getTestContext(), notifier, UPDATE_DELAY_FOR_TEST);
        dService.setDownloadSnackbarController(snackbarController);
        // Try calling download completed directly.
        DownloadInfo successful = Builder.fromDownloadInfo(getDownloadInfo())
                .setIsSuccessful(true).build();

        // First the progress notification should be removed and then
        // a new successful notification should be added.
        notifier.expect(MethodID.CANCEL_DOWNLOAD_ID, successful.getDownloadId())
                .andThen(MethodID.DOWNLOAD_SUCCESSFUL, successful);

        dService.onDownloadCompleted(successful);
        notifier.waitTillExpectedCallsComplete();
        snackbarController.waitForSnackbarControllerToFinish(true);

        // Now check that a cancel works.
        DownloadInfo failure = Builder.fromDownloadInfo(getDownloadInfo())
                .setIsSuccessful(false).build();
        notifier.expect(MethodID.CANCEL_DOWNLOAD_ID, failure.getDownloadId())
                .andThen(MethodID.DOWNLOAD_FAILED, failure);
        dService.onDownloadCompleted(failure);
        notifier.waitTillExpectedCallsComplete();
        snackbarController.waitForSnackbarControllerToFinish(false);

        // Now check that a successful notification appears after a download progress.
        DownloadInfo progress = getDownloadInfo();
        successful = Builder.fromDownloadInfo(progress)
                .setIsSuccessful(true).build();
        notifier.expect(MethodID.DOWNLOAD_PROGRESS, progress)
                .andThen(MethodID.CANCEL_DOWNLOAD_ID, progress.getDownloadId())
                .andThen(MethodID.DOWNLOAD_SUCCESSFUL, successful);
        dService.onDownloadUpdated(progress);
        Thread.sleep(DELAY_BETWEEN_CALLS);
        dService.onDownloadCompleted(successful);
        notifier.waitTillExpectedCallsComplete();
        snackbarController.waitForSnackbarControllerToFinish(true);
    }

    @MediumTest
    @Feature({"Download"})
    public void testMultipleDownloadProgress() {
        MockDownloadNotifier notifier = new MockDownloadNotifier();
        DownloadManagerServiceForTest dService = new DownloadManagerServiceForTest(
                getTestContext(), notifier, UPDATE_DELAY_FOR_TEST);

        DownloadInfo download1 = getDownloadInfo();
        DownloadInfo download2 = getDownloadInfo();
        DownloadInfo download3 = getDownloadInfo();
        OneTimeMatchSet matchSet = new OneTimeMatchSet(download1, download2, download3);
        notifier.expect(MethodID.DOWNLOAD_PROGRESS, matchSet)
                .andThen(MethodID.DOWNLOAD_PROGRESS, matchSet)
                .andThen(MethodID.DOWNLOAD_PROGRESS, matchSet);
        dService.onDownloadUpdated(download1);
        dService.onDownloadUpdated(download2);
        dService.onDownloadUpdated(download3);

        notifier.waitTillExpectedCallsComplete();
        assertTrue("All downloads should be updated.", matchSet.mMatches.isEmpty());
    }

    @MediumTest
    @Feature({"Download"})
    public void testInProgressDownloadsAreCancelled() {
        MockDownloadNotifier notifier = new MockDownloadNotifier();
        DownloadManagerServiceForTest dService = new DownloadManagerServiceForTest(
                getTestContext(), notifier, UPDATE_DELAY_FOR_TEST);

        DownloadInfo download1 = getDownloadInfo();
        DownloadInfo download2 = getDownloadInfo();
        DownloadInfo download3 = getDownloadInfo();
        OneTimeMatchSet matchSet = new OneTimeMatchSet(download1, download2, download3);
        notifier.expect(MethodID.DOWNLOAD_PROGRESS, matchSet)
                .andThen(MethodID.DOWNLOAD_PROGRESS, matchSet)
                .andThen(MethodID.DOWNLOAD_PROGRESS, matchSet);

        dService.onDownloadUpdated(download1);
        dService.onDownloadUpdated(download2);
        dService.onDownloadUpdated(download3);

        notifier.waitTillExpectedCallsComplete();
        assertTrue("All downloads should be updated.", matchSet.mMatches.isEmpty());

        // Check if notifications are removed when clearPendingNotifications is called.
        dService.clearPendingDownloadNotifications();
        Set<String> downloads = dService.getStoredDownloadInfo(
                PreferenceManager.getDefaultSharedPreferences(getTestContext()),
                DownloadManagerService.PENDING_DOWNLOAD_NOTIFICATIONS);
        assertTrue("All downloads should be removed.", downloads.isEmpty());
    }

    /**
     * Test to make sure {@link DownloadManagerService#clearPendingDownloadNotifications}
     * will clear the OMA notifications and pass the notification URI to {@link OMADownloadHandler}.
     */
    @MediumTest
    @Feature({"Download"})
    public void testClearPendingOMADownloads() throws InterruptedException {
        DownloadManager manager =
                (DownloadManager) getTestContext().getSystemService(Context.DOWNLOAD_SERVICE);
        long downloadId = manager.addCompletedDownload(
                "test", "test", false, "text/html",
                UrlUtils.getIsolatedTestFilePath("chrome/test/data/android/download/download.txt"),
                4, true);
        MockDownloadNotifier notifier = new MockDownloadNotifier();
        DownloadManagerServiceForTest dService = new DownloadManagerServiceForTest(
                getTestContext(), notifier, UPDATE_DELAY_FOR_TEST);
        final MockOMADownloadHandler handler = new MockOMADownloadHandler(getTestContext());
        dService.setOMADownloadHandler(handler);
        dService.addOMADownloadToSharedPrefs(String.valueOf(downloadId) + "," + INSTALL_NOTIFY_URI);
        dService.clearPendingDownloadNotifications();
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return handler.mSuccess;
            }
        });
        assertEquals(handler.mNofityURI, "http://test/test");
        manager.remove(downloadId);
    }

    /**
     * Test that calling {@link DownloadManagerService#enqueueDownloadManagerRequest} for an
     * OMA download will enqueue a new DownloadManager request and insert an entry into the
     * SharedPrefs.
     */
    @MediumTest
    @Feature({"Download"})
    public void testEnqueueOMADownloads() throws InterruptedException {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());

        try {
            DownloadInfo info = new DownloadInfo.Builder()
                    .setDownloadId(0)
                    .setMimeType(OMADownloadHandler.OMA_DRM_MESSAGE_MIME)
                    .setFileName("test.gzip")
                    .setUrl(testServer.getURL("/chrome/test/data/android/download/test.gzip"))
                    .build();
            MockDownloadNotifier notifier = new MockDownloadNotifier();
            Context context = getTestContext();
            DownloadManagerServiceForTest dService = new DownloadManagerServiceForTest(
                    context, notifier, UPDATE_DELAY_FOR_TEST);
            final MockOMADownloadHandler handler = new MockOMADownloadHandler(context);
            dService.setOMADownloadHandler(handler);
            handler.setDownloadId(0);
            dService.enqueueDownloadManagerRequest(info, true);
            CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return handler.mDownloadId != 0;
                }
            });
            handler.mDownloadId = handler.mDownloadInfo.getDownloadId();
            Set<String> downloads = dService.getStoredDownloadInfo(
                    PreferenceManager.getDefaultSharedPreferences(context),
                    DownloadManagerService.PENDING_OMA_DOWNLOADS);
            assertEquals(1, downloads.size());
            DownloadManagerService.OMAEntry entry = DownloadManagerService.OMAEntry.parseOMAEntry(
                    (String) (downloads.toArray()[0]));
            assertEquals(entry.mDownloadId, handler.mDownloadId);
            assertEquals(entry.mInstallNotifyURI, INSTALL_NOTIFY_URI);
            DownloadManager manager =
                    (DownloadManager) getTestContext().getSystemService(Context.DOWNLOAD_SERVICE);
            manager.remove(handler.mDownloadId);
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    /**
     * Test to make sure {@link DownloadManagerService#shouldOpenAfterDownload}
     * returns the right result for varying MIME types and Content-Dispositions.
     */
    @SmallTest
    @Feature({"Download"})
    public void testShouldOpenAfterDownload() {
        // Should not open any download type MIME types.
        assertFalse(
                DownloadManagerService.shouldOpenAfterDownload(new DownloadInfo.Builder()
                        .setMimeType("application/download")
                        .setHasUserGesture(true)
                        .build()));
        assertFalse(
                DownloadManagerService.shouldOpenAfterDownload(new DownloadInfo.Builder()
                        .setMimeType("application/x-download")
                        .setHasUserGesture(true)
                        .build()));
        assertFalse(
                DownloadManagerService.shouldOpenAfterDownload(new DownloadInfo.Builder()
                        .setMimeType("application/octet-stream")
                        .setHasUserGesture(true)
                        .build()));

        // Should open PDFs.
        assertTrue(
                DownloadManagerService.shouldOpenAfterDownload(new DownloadInfo.Builder()
                        .setMimeType("application/pdf")
                        .setHasUserGesture(true)
                        .build()));
        assertTrue(
                DownloadManagerService.shouldOpenAfterDownload(new DownloadInfo.Builder()
                        .setContentDisposition("filename=test.pdf")
                        .setMimeType("application/pdf")
                        .setHasUserGesture(true)
                        .build()));

        // Require user gesture.
        assertFalse(
                DownloadManagerService.shouldOpenAfterDownload(new DownloadInfo.Builder()
                        .setMimeType("application/pdf")
                        .setHasUserGesture(false)
                        .build()));
        assertFalse(
                DownloadManagerService.shouldOpenAfterDownload(new DownloadInfo.Builder()
                        .setContentDisposition("filename=test.pdf")
                        .setMimeType("application/pdf")
                        .setHasUserGesture(false)
                        .build()));

        // But should not open any PDFs with Content-Disposition: attachment.
        assertFalse(
                DownloadManagerService.shouldOpenAfterDownload(new DownloadInfo.Builder()
                        .setContentDisposition("attachment")
                        .setMimeType("application/pdf")
                        .setHasUserGesture(true)
                        .build()));
        assertFalse(
                DownloadManagerService.shouldOpenAfterDownload(new DownloadInfo.Builder()
                        .setContentDisposition("attachment; filename=test.pdf")
                        .setMimeType("application/pdf")
                        .setHasUserGesture(true)
                        .build()));
    }

    @SmallTest
    @Feature({"Download"})
    public void testParseDownloadNotifications() {
        String notification = "1,0,test.pdf";
        DownloadManagerService.PendingNotification pendingNotification =
                DownloadManagerService.PendingNotification.parseFromString(notification);
        assertEquals(1, pendingNotification.downloadId);
        assertEquals("test.pdf", pendingNotification.fileName);
        assertFalse(pendingNotification.isResumable);

        notification = "2,1,test,2.pdf";
        pendingNotification =
                DownloadManagerService.PendingNotification.parseFromString(notification);
        assertEquals(2, pendingNotification.downloadId);
        assertEquals("test,2.pdf", pendingNotification.fileName);
        assertTrue(pendingNotification.isResumable);
    }
}
