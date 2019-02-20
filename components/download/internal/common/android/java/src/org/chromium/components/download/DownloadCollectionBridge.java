// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.download;

import android.content.ContentResolver;
import android.net.Uri;
import android.os.ParcelFileDescriptor;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Helper class for publishing download files to the public download collection.
 */
@JNINamespace("download")
public class DownloadCollectionBridge {
    // Singleton instance that allows embedders to replace their implementation.
    private static DownloadCollectionBridge sDownloadCollectionBridge;
    private static final String TAG = "DownloadCollection";

    /**
     * Return getDownloadCollectionBridge singleton.
     */
    public static DownloadCollectionBridge getDownloadCollectionBridge() {
        ThreadUtils.assertOnUiThread();
        if (sDownloadCollectionBridge == null) {
            sDownloadCollectionBridge = new DownloadCollectionBridge();
        }
        return sDownloadCollectionBridge;
    }

    /**
     * Sets the singlton object to use later.
     */
    public static void setDownloadCollectionBridge(DownloadCollectionBridge bridge) {
        ThreadUtils.assertOnUiThread();
        sDownloadCollectionBridge = bridge;
    }

    /**
     * Returns whether a download needs to be published.
     * @param filePath File path of the download.
     * @return True if the download needs to be published, or false otherwise.
     */
    protected boolean needToPublishDownload(final String filePath) {
        return false;
    }

    /**
     * Creates a pending session for download to be written into.
     * @param fileName Name of the file.
     * @param mimeType Mime type of the file.
     * @param originalUrl Originating URL of the download.
     * @param referrer Referrer of the download.
     * @return Uri created for the pending session.
     */
    protected Uri createPendingSession(final String fileName, final String mimeType,
            final String originalUrl, final String referrer) {
        return null;
    }

    /**
     * Copy file content from a source file to the pending Uri.
     * @param sourcePath File content to be copied from.
     * @param pendingUri Destination Uri to be copied to.
     * @return true on success, or false otherwise.
     */
    protected boolean copyFileToPendingUri(final String sourcePath, final String pendingUri) {
        return false;
    }

    /**
     * Abandon the the intermediate Uri.
     * @param pendingUri Intermediate Uri that is going to be deleted.
     */
    protected void abandonPendingUri(final String pendingUri) {}

    /**
     * Publish a completed download to public repository.
     * @param pendingUri Pending uri to publish.
     * @return Uri of the published file.
     */
    protected Uri publishCompletedDownload(final String pendingUri) {
        return null;
    }

    /**
     * Creates an intermediate URI for download to be written into. On completion, call
     * nativeOnCreateIntermediateUriResult() with |callbackId|.
     * @param fileName Name of the file.
     * @param mimeType Mime type of the file.
     * @param originalUrl Originating URL of the download.
     * @param referrer Referrer of the download.
     */
    @CalledByNative
    private static String createIntermediateUriForPublish(final String fileName,
            final String mimeType, final String originalUrl, final String referrer) {
        Uri uri = getDownloadCollectionBridge().createPendingSession(
                fileName, mimeType, originalUrl, referrer);
        return uri == null ? null : uri.toString();
    }

    /**
     * Returns whether a download needs to be published.
     * @param filePath File path of the download.
     * @return True if the download needs to be published, or false otherwise.
     */
    @CalledByNative
    private static boolean shouldPublishDownload(final String filePath) {
        return getDownloadCollectionBridge().needToPublishDownload(filePath);
    }

    /**
     * Copies file content from a source file to the destination Uri.
     * @param sourcePath File content to be copied from.
     * @param destinationUri Destination Uri to be copied to.
     * @return True on success, or false otherwise.
     */
    @CalledByNative
    private static boolean copyFileToIntermediateUri(
            final String sourcePath, final String destinationUri) {
        return getDownloadCollectionBridge().copyFileToPendingUri(sourcePath, destinationUri);
    }

    /**
     * Deletes the intermediate Uri.
     * @param uri Intermediate Uri that is going to be deleted.
     */
    @CalledByNative
    private static void deleteIntermediateUri(final String uri) {
        getDownloadCollectionBridge().abandonPendingUri(uri);
    }

    /**
     * Publishes the completed download to public download collection.
     * @param intermediateUri Intermediate Uri that is going to be published.
     * @return Uri of the published file.
     */
    @CalledByNative
    private static String publishDownload(final String intermediateUri) {
        Uri uri = getDownloadCollectionBridge().publishCompletedDownload(intermediateUri);
        return uri == null ? null : uri.toString();
    }

    /**
     * Opens the intermediate Uri for writing.
     * @param intermediateUri Intermediate Uri that is going to be written to.
     * @return file descriptor that is opened for writing.
     */
    @CalledByNative
    private static int openIntermediateUri(final String intermediateUri) {
        try {
            ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
            ParcelFileDescriptor pfd =
                    resolver.openFileDescriptor(Uri.parse(intermediateUri), "rw");
            return pfd.detachFd();
        } catch (Exception e) {
            Log.e(TAG, "Cannot open intermediate Uri.", e);
        }
        return -1;
    }
}
