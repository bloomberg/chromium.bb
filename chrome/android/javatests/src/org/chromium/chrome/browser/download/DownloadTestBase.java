// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.DownloadManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.Cursor;
import android.os.Environment;
import android.os.Handler;
import android.util.Log;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.DownloadController;
import org.chromium.content.browser.DownloadInfo;
import org.chromium.content.browser.test.util.ApplicationUtils;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Base case for tests that need to download a file.
 *
 * This has to be a base class because some classes (like BrowserEvent) are exposed only
 * to children of ChromeActivityTestCaseBase. It is a very broken approach to sharing
 * but the only other option is to refactor the ChromeActivityTestCaseBase implementation
 * and all of our test cases.
 *
 */
public abstract class DownloadTestBase extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TAG = "DownloadTestBase";
    private static final File DOWNLOAD_DIRECTORY =
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
    public static final long UPDATE_DELAY_MILLIS = 1000;

    public DownloadTestBase() {
        super(ChromeActivity.class);
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
                        assertEquals(stream.read(data), data.length);
                        String contents = new String(data);
                        assertEquals(expectedContents, contents);
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
        assertTrue(isSameDownloadFile(fileName, lastDownload));
        assertTrue(hasDownload(lastDownload, null));
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

            if (fileName != null) {  // Somehow fileName can be null for some entries.
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

    private CallbackHelper mHttpGetDownloadFinished;
    private String mLastDownloadFilePath;
    private CallbackHelper mHttpDownloadFinished;
    private DownloadManagerService mSavedDownloadManagerService;

    protected String getLastDownloadFile() {
        return new File(mLastDownloadFilePath).getName();
    }

    // The Android DownloadManager sometimes appends a number to a file name when it downloads it
    // ex: google.png becomes google-23.png
    // This happens even when there is no other prior download with that name, it could be a bug.
    // TODO(jcivelli): investigate if we can isolate that behavior and file a bug to Android.
    protected boolean isSameDownloadFile(String originalName, String downloadName) {
        String fileName = originalName;
        String extension = "";
        int dotIndex = originalName.lastIndexOf('.');
        if (dotIndex != -1 && dotIndex < originalName.length()) {
            fileName = originalName.substring(0, dotIndex);
            extension = originalName.substring(dotIndex);  // We include the '.'
        }
        return downloadName.startsWith(fileName) && downloadName.endsWith(extension);
    }

    public boolean waitForGetDownloadToFinish() throws InterruptedException {
        mLastDownloadFilePath = null;
        boolean eventReceived = true;
        try {
            mHttpGetDownloadFinished.waitForCallback(0);
        } catch (TimeoutException e) {
            eventReceived = false;
        }
        mHttpGetDownloadFinished = new CallbackHelper();
        return eventReceived;
    }

    public boolean waitForChromeDownloadToFinish() throws InterruptedException {
        boolean eventReceived = true;
        try {
            mHttpDownloadFinished.waitForCallback(0, 1, 5, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            eventReceived = false;
        }
        mHttpDownloadFinished = new CallbackHelper();
        return eventReceived;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        cleanUpAllDownloads();

        try {
            assertTrue(ApplicationUtils.waitForLibraryDependencies(getInstrumentation()));
        } catch (InterruptedException e) {
            fail("Library dependencies were never initialized.");
        }
        final Context context = getInstrumentation().getTargetContext().getApplicationContext();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSavedDownloadManagerService = DownloadManagerService.setDownloadManagerService(
                        new DownloadManagerService(context,
                                new SystemDownloadNotifier(context), new Handler(),
                                UPDATE_DELAY_MILLIS) {
                            @Override
                            protected void broadcastDownloadSuccessful(DownloadInfo downloadInfo) {
                                super.broadcastDownloadSuccessful(downloadInfo);
                                mLastDownloadFilePath = downloadInfo.getFilePath();
                                mHttpDownloadFinished.notifyCalled();
                            }
                        });
                DownloadController.setDownloadNotificationService(
                        DownloadManagerService.getDownloadManagerService(context));
            }
        });

        mHttpGetDownloadFinished = new CallbackHelper();
        mHttpDownloadFinished = new CallbackHelper();

        context.registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                String action = intent.getAction();
                if (DownloadManager.ACTION_DOWNLOAD_COMPLETE.equals(action)) {
                    // The name of the download does not seem to always be what we asked for
                    // (ex: google.png becomes google-23.png). Probably a bug in the Android
                    // DownloadManager.  Make sure we always retrieve the actual download name.
                    long downloadId = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, -1);
                    assertTrue("No download id with download finished intent", downloadId != -1);
                    mLastDownloadFilePath = getPathForDownload(downloadId);
                    assertNotNull("Download path not found", mLastDownloadFilePath);
                    mHttpGetDownloadFinished.notifyCalled();
                }
            }
        }, new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE));
    }

    @Override
    protected void tearDown() throws Exception {
        cleanUpAllDownloads();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                DownloadManagerService.setDownloadManagerService(mSavedDownloadManagerService);
                DownloadController.setDownloadNotificationService(mSavedDownloadManagerService);
            }
        });
        super.tearDown();
    }
}
