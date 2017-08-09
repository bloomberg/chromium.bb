// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.Manifest.permission;
import android.app.DownloadManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Environment;
import android.text.TextUtils;
import android.util.Pair;
import android.webkit.MimeTypeMap;
import android.webkit.URLUtil;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.PermissionCallback;

import java.io.File;

/**
 * Chrome implementation of the ContentViewDownloadDelegate interface.
 *
 * Listens to POST and GET download events. GET download requests are passed along to the
 * Android Download Manager. POST downloads are expected to be handled natively and listener
 * is responsible for adding the completed download to the download manager.
 *
 * Prompts the user when a dangerous file is downloaded. Auto-opens PDFs after downloading.
 */
public class ChromeDownloadDelegate {
    private static final String TAG = "Download";

    // The application context.
    private final Context mContext;
    private Tab mTab;

    /**
     * Creates ChromeDownloadDelegate.
     * @param context The application context.
     * @param tab The corresponding tab instance.
     */
    public ChromeDownloadDelegate(Context context, Tab tab) {
        mContext = context;
        mTab = tab;
        mTab.addObserver(new EmptyTabObserver() {
            @Override
            public void onDestroyed(Tab tab) {
                mTab = null;
            }
        });
    }

    /**
     * Notify the host application a download should be done, even if there is a
     * streaming viewer available for this type.
     *
     * @param downloadInfo Information about the download.
     */
    protected void onDownloadStartNoStream(final DownloadInfo downloadInfo) {
        final String fileName = downloadInfo.getFileName();
        assert !TextUtils.isEmpty(fileName);
        final String newMimeType =
                remapGenericMimeType(downloadInfo.getMimeType(), downloadInfo.getUrl(), fileName);
        new AsyncTask<Void, Void, Pair<String, File>>() {
            @Override
            protected Pair<String, File> doInBackground(Void... params) {
                // Check to see if we have an SDCard.
                String status = Environment.getExternalStorageState();
                File fullDirPath = getDownloadDirectoryFullPath();
                return new Pair<String, File>(status, fullDirPath);
            }

            @Override
            protected void onPostExecute(Pair<String, File> result) {
                String externalStorageState = result.first;
                File fullDirPath = result.second;
                if (!checkExternalStorageAndNotify(
                        fileName, fullDirPath, externalStorageState)) {
                    return;
                }
                String url = sanitizeDownloadUrl(downloadInfo);
                if (url == null) return;
                DownloadInfo newInfo = DownloadInfo.Builder.fromDownloadInfo(downloadInfo)
                                               .setUrl(url)
                                               .setMimeType(newMimeType)
                                               .setDescription(url)
                                               .setFileName(fileName)
                                               .setIsGETRequest(true)
                                               .build();
                DownloadController.enqueueDownloadManagerRequest(newInfo);
                DownloadController.closeTabIfBlank(mTab);
            }
        }.execute();
    }

    /**
     * Sanitize the URL for the download item.
     *
     * @param downloadInfo Information about the download.
     */
    protected String sanitizeDownloadUrl(DownloadInfo downloadInfo) {
        return downloadInfo.getUrl();
    }

    /**
     * Return the full path of the download directory.
     *
     * @return File object containing the path to the download directory.
     */
    private static File getDownloadDirectoryFullPath() {
        assert !ThreadUtils.runningOnUiThread();
        File dir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        if (!dir.mkdir() && !dir.isDirectory()) return null;
        return dir;
    }

    private static boolean checkFileExists(File dirPath, final String fileName) {
        assert !ThreadUtils.runningOnUiThread();
        final File file = new File(dirPath, fileName);
        return file != null && file.exists();
    }

    private static void deleteFileForOverwrite(DownloadInfo info) {
        assert !ThreadUtils.runningOnUiThread();
        File dir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        if (!dir.isDirectory()) return;
        final File file = new File(dir, info.getFileName());
        if (!file.delete()) {
            Log.e(TAG, "Failed to delete a file: " + info.getFileName());
        }
    }

    /**
     * Enqueue download manager request, only from native side. Note that at this point
     * we don't need to show an infobar even when the file already exists.
     *
     * @param overwrite Whether or not we will overwrite the file.
     * @param downloadInfo The download info.
     * @return true iff this request resulted in the tab creating the download to close.
     */
    @CalledByNative
    private boolean enqueueDownloadManagerRequestFromNative(
            final boolean overwrite, final DownloadInfo downloadInfo) {
        if (overwrite) {
            // Android DownloadManager does not have an overwriting option.
            // We remove the file here instead.
            new AsyncTask<Void, Void, Void>() {
                @Override
                public Void doInBackground(Void... params) {
                    deleteFileForOverwrite(downloadInfo);
                    return null;
                }

                @Override
                public void onPostExecute(Void args) {
                    DownloadController.enqueueDownloadManagerRequest(downloadInfo);
                }
            }.execute();
        } else {
            DownloadController.enqueueDownloadManagerRequest(downloadInfo);
        }
        return DownloadController.closeTabIfBlank(mTab);
    }

    /**
     * Check the external storage and notify user on error.
     *
     * @param fullDirPath The dir path to download a file. Normally this is external storage.
     * @param externalStorageStatus The status of the external storage.
     * @return Whether external storage is ok for downloading.
     */
    private boolean checkExternalStorageAndNotify(
            String filename, File fullDirPath, String externalStorageStatus) {
        if (fullDirPath == null) {
            Log.e(TAG, "Download failed: no SD card");
            alertDownloadFailure(
                    filename, DownloadManager.ERROR_DEVICE_NOT_FOUND);
            return false;
        }
        if (!externalStorageStatus.equals(Environment.MEDIA_MOUNTED)) {
            int reason = DownloadManager.ERROR_DEVICE_NOT_FOUND;
            // Check to see if the SDCard is busy, same as the music app
            if (externalStorageStatus.equals(Environment.MEDIA_SHARED)) {
                Log.e(TAG, "Download failed: SD card unavailable");
                reason = DownloadManager.ERROR_FILE_ERROR;
            } else {
                Log.e(TAG, "Download failed: no SD card");
            }
            alertDownloadFailure(filename, reason);
            return false;
        }
        return true;
    }

    /**
     * Alerts user of download failure.
     *
     * @param fileName Name of the download file.
     * @param reason Reason of failure defined in {@link DownloadManager}
     */
    private void alertDownloadFailure(String fileName, int reason) {
        DownloadManagerService.getDownloadManagerService().onDownloadFailed(fileName, reason);
    }

    /**
     * If the given MIME type is null, or one of the "generic" types (text/plain
     * or application/octet-stream) map it to a type that Android can deal with.
     * If the given type is not generic, return it unchanged.
     *
     * We have to implement this ourselves as
     * MimeTypeMap.remapGenericMimeType() is not public.
     * See http://crbug.com/407829.
     *
     * @param mimeType MIME type provided by the server.
     * @param url URL of the data being loaded.
     * @param filename file name obtained from content disposition header
     * @return The MIME type that should be used for this data.
     */
    static String remapGenericMimeType(String mimeType, String url, String filename) {
        // If we have one of "generic" MIME types, try to deduce
        // the right MIME type from the file extension (if any):
        if (mimeType == null || mimeType.isEmpty() || "text/plain".equals(mimeType)
                || "application/octet-stream".equals(mimeType)
                || "binary/octet-stream".equals(mimeType)
                || "octet/stream".equals(mimeType)
                || "application/force-download".equals(mimeType)
                || "application/unknown".equals(mimeType)) {

            String extension = getFileExtension(url, filename);
            String newMimeType = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
            if (newMimeType != null) {
                mimeType = newMimeType;
            } else if (extension.equals("dm")) {
                mimeType = OMADownloadHandler.OMA_DRM_MESSAGE_MIME;
            } else if (extension.equals("dd")) {
                mimeType = OMADownloadHandler.OMA_DOWNLOAD_DESCRIPTOR_MIME;
            }
        }
        return mimeType;
    }

    /**
     * Retrieve the file extension from a given file name or url.
     *
     * @param url URL to extract the extension.
     * @param filename File name to extract the extension.
     * @return If extension can be extracted from file name, use that. Or otherwise, use the
     *         extension extracted from the url.
     */
    static String getFileExtension(String url, String filename) {
        if (!TextUtils.isEmpty(filename)) {
            int index = filename.lastIndexOf(".");
            if (index > 0) return filename.substring(index + 1);
        }
        return MimeTypeMap.getFileExtensionFromUrl(url);
    }

    /**
     * For certain download types(OMA for example), android DownloadManager should
     * handle them. Call this function to intercept those downloads.
     *
     * @param url URL to be downloaded.
     * @return whether the DownloadManager should intercept the download.
     */
    public boolean shouldInterceptContextMenuDownload(String url) {
        Uri uri = Uri.parse(url);
        String scheme = uri.normalizeScheme().getScheme();
        if (!UrlConstants.HTTP_SCHEME.equals(scheme) && !UrlConstants.HTTPS_SCHEME.equals(scheme)) {
            return false;
        }
        String path = uri.getPath();
        if (!OMADownloadHandler.isOMAFile(path)) return false;
        if (mTab == null) return true;
        String fileName = URLUtil.guessFileName(
                url, null, OMADownloadHandler.OMA_DRM_MESSAGE_MIME);
        final DownloadInfo downloadInfo =
                new DownloadInfo.Builder().setUrl(url).setFileName(fileName).build();
        WindowAndroid window = mTab.getWindowAndroid();
        if (window.hasPermission(permission.WRITE_EXTERNAL_STORAGE)) {
            onDownloadStartNoStream(downloadInfo);
        } else if (window.canRequestPermission(permission.WRITE_EXTERNAL_STORAGE)) {
            PermissionCallback permissionCallback = (permissions, grantResults) -> {
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    onDownloadStartNoStream(downloadInfo);
                }
            };
            window.requestPermissions(
                    new String[] {permission.WRITE_EXTERNAL_STORAGE}, permissionCallback);
        }
        return true;
    }

    protected Context getContext() {
        return mContext;
    }
}
