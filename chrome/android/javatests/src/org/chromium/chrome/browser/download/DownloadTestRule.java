// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.DownloadManager;
import android.content.Context;
import android.database.Cursor;
import android.os.Environment;
import android.os.Handler;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content.browser.test.util.ApplicationUtils;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Custom TestRule for tests that need to download a file.
 *
 * This has to be a base class because some classes (like BrowserEvent) are exposed only
 * to children of ChromeActivityTestCaseBase. It is a very broken approach to sharing
 * but the only other option is to refactor the ChromeActivityTestCaseBase implementation
 * and all of our test cases.
 *
 */
public class DownloadTestRule extends ChromeActivityTestRule<ChromeActivity> {
    private static final String TAG = "DownloadTestBase";
    private static final File DOWNLOAD_DIRECTORY =
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
    public static final long UPDATE_DELAY_MILLIS = 1000;

    private final CustomMainActivityStart mActivityStart;

    public DownloadTestRule(CustomMainActivityStart action) {
        super(ChromeActivity.class);
        mActivityStart = action;
    }

    /**
     * Check the download exists in DownloadManager by matching the local file
     * path.
     *
     * @param fileName Expected file name. Path is built by appending filename to
     * the system downloads path.
     *
     * @param expectedContents Expected contents of the file, or null if the contents should not be
     * checked.
     */
    public boolean hasDownload(String fileName, String expectedContents) throws IOException {
        File downloadedFile = new File(DOWNLOAD_DIRECTORY, fileName);
        if (!downloadedFile.exists()) {
            Log.d(TAG, "The file " + fileName + " does not exist");
            return false;
        }

        String fullPath = downloadedFile.getAbsolutePath();

        DownloadManager manager =
                (DownloadManager) getActivity().getSystemService(Context.DOWNLOAD_SERVICE);
        Cursor cursor = manager.query(new DownloadManager.Query());

        cursor.moveToFirst();
        boolean result = false;
        while (!cursor.isAfterLast()) {
            if (fullPath.equals(getPathFromCursor(cursor))) {
                if (expectedContents != null) {
                    FileInputStream stream = new FileInputStream(new File(fullPath));
                    byte[] data = new byte[expectedContents.getBytes().length];
                    try {
                        Assert.assertEquals(stream.read(data), data.length);
                        String contents = new String(data);
                        Assert.assertEquals(expectedContents, contents);
                    } finally {
                        stream.close();
                    }
                }
                result = true;
                break;
            }
            cursor.moveToNext();
        }
        cursor.close();
        return result;
    }

    /**
     * Check the last download matches the given name and exists in DownloadManager.
     */
    public void checkLastDownload(String fileName) throws IOException {
        String lastDownload = getLastDownloadFile();
        Assert.assertTrue(isSameDownloadFile(fileName, lastDownload));
        Assert.assertTrue(hasDownload(lastDownload, null));
    }

    /**
     * Delete all download entries in DownloadManager and delete the corresponding files.
     */
    @SuppressFBWarnings("RV_RETURN_VALUE_IGNORED_BAD_PRACTICE")
    private void cleanUpAllDownloads() {
        DownloadManager manager =
                (DownloadManager) getActivity().getSystemService(Context.DOWNLOAD_SERVICE);
        Cursor cursor = manager.query(new DownloadManager.Query());
        int idColumnIndex = cursor.getColumnIndexOrThrow(DownloadManager.COLUMN_ID);
        cursor.moveToFirst();
        while (!cursor.isAfterLast()) {
            long id = cursor.getLong(idColumnIndex);
            String fileName = getPathFromCursor(cursor);
            manager.remove(id);

            if (fileName != null) { // Somehow fileName can be null for some entries.
                // manager.remove does not remove downloaded file.
                File localFile = new File(fileName);
                if (localFile.exists()) {
                    localFile.delete();
                }
            }

            cursor.moveToNext();
        }
        cursor.close();
    }

    /**
     * Retrieve the path of the downloaded file from a DownloadManager cursor.
     */
    private String getPathFromCursor(Cursor cursor) {
        int columnId = cursor.getColumnIndex("local_filename");
        return cursor.getString(columnId);
    }

    private String getPathForDownload(long downloadId) {
        DownloadManager manager =
                (DownloadManager) getActivity().getSystemService(Context.DOWNLOAD_SERVICE);
        DownloadManager.Query query = new DownloadManager.Query();
        query.setFilterById(downloadId);
        Cursor cursor = null;
        try {
            cursor = manager.query(query);
            if (!cursor.moveToFirst()) {
                return null;
            }
            return getPathFromCursor(cursor);
        } finally {
            if (cursor != null) cursor.close();
        }
    }

    private String mLastDownloadFilePath;
    private final CallbackHelper mHttpDownloadFinished = new CallbackHelper();
    private DownloadManagerService mSavedDownloadManagerService;

    public String getLastDownloadFile() {
        return new File(mLastDownloadFilePath).getName();
    }

    // The Android DownloadManager sometimes appends a number to a file name when it downloads it
    // ex: google.png becomes google-23.png
    // This happens even when there is no other prior download with that name, it could be a bug.
    // TODO(jcivelli): investigate if we can isolate that behavior and file a bug to Android.
    public boolean isSameDownloadFile(String originalName, String downloadName) {
        String fileName = originalName;
        String extension = "";
        int dotIndex = originalName.lastIndexOf('.');
        if (dotIndex != -1 && dotIndex < originalName.length()) {
            fileName = originalName.substring(0, dotIndex);
            extension = originalName.substring(dotIndex); // We include the '.'
        }
        return downloadName.startsWith(fileName) && downloadName.endsWith(extension);
    }

    public int getChromeDownloadCallCount() {
        return mHttpDownloadFinished.getCallCount();
    }

    public boolean waitForChromeDownloadToFinish(int callCount) throws InterruptedException {
        boolean eventReceived = true;
        try {
            mHttpDownloadFinished.waitForCallback(callCount, 1, 5, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            eventReceived = false;
        }
        return eventReceived;
    }

    private class TestDownloadManagerService extends DownloadManagerService {
        public TestDownloadManagerService(Context context, DownloadNotifier downloadNotifier,
                Handler handler, long updateDelayInMillis) {
            super(context, downloadNotifier, handler, updateDelayInMillis);
        }

        @Override
        public void broadcastDownloadSuccessful(DownloadInfo downloadInfo) {
            super.broadcastDownloadSuccessful(downloadInfo);
            mLastDownloadFilePath = downloadInfo.getFilePath();
            mHttpDownloadFinished.notifyCalled();
        }
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUp();
                base.evaluate();
                tearDown();
            }
        }, description);
    }

    private void setUp() throws Exception {
        mActivityStart.customMainActivityStart();

        cleanUpAllDownloads();

        ApplicationUtils.waitForLibraryDependencies(getInstrumentation());
        final Context context = getInstrumentation().getTargetContext().getApplicationContext();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSavedDownloadManagerService = DownloadManagerService.setDownloadManagerService(
                        new TestDownloadManagerService(context, new SystemDownloadNotifier(context),
                                new Handler(), UPDATE_DELAY_MILLIS));
                DownloadController.setDownloadNotificationService(
                        DownloadManagerService.getDownloadManagerService());
            }
        });
    }

    private void tearDown() throws Exception {
        cleanUpAllDownloads();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                DownloadManagerService.setDownloadManagerService(mSavedDownloadManagerService);
                DownloadController.setDownloadNotificationService(mSavedDownloadManagerService);
            }
        });
    }

    public void deleteFilesInDownloadDirectory(String... filenames) {
        for (String filename : filenames) {
            final File fileToDelete = new File(DOWNLOAD_DIRECTORY, filename);
            if (fileToDelete.exists()) {
                Assert.assertTrue(
                        "Could not delete file that would block this test", fileToDelete.delete());
            }
        }
    }

    /**
     * Interface for Download tests to define actions that starts the activity.
     *
     * This method will be called in DownloadTestRule's setUp process, which means
     * it would happen before Test class' own setUp() call
     **/
    public interface CustomMainActivityStart {
        void customMainActivityStart() throws InterruptedException;
    }
}
