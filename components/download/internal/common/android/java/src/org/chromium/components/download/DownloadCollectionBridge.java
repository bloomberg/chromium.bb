// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.download;

import android.net.Uri;

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
    protected boolean needToPublishDownload(String filePath) {
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
    protected boolean copyFileToPendingUri(String sourcePath, String pendingUri) {
        return false;
    }

    /**
     * Abandon the the intermediate Uri.
     * @param pendingUri Intermediate Uri that is going to be deleted.
     */
    protected void abandonPendingUri(String pendingUri) {}

    /**
     * Publish a completed download to public repository.
     * @param pendingUri Pending uri to publish.

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
    private static boolean shouldPublishDownload(String filePath) {
        return getDownloadCollectionBridge().needToPublishDownload(filePath);
    }

    /**
     * Copy file content from a source file to the destination Uri.
     * @param sourcePath File content to be copied from.
     * @param destinationUri Destination Uri to be copied to.
     * @return True on success, or false otherwise.
     */
    @CalledByNative
    private static boolean copyFileToIntermediateUri(String sourcePath, String destinationUri) {
        return getDownloadCollectionBridge().copyFileToPendingUri(sourcePath, destinationUri);
    }
}
