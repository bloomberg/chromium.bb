// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

/**
 * Interface to be implemented by the embedder to handle file downloads.
 */
public interface ContentViewDownloadDelegate {
    /**
    * Notify the host application that a file should be downloaded. Replaces
    * onDownloadStart from DownloadListener.
    * @param downloadInfo Information about the requested download.
    * @param mustDownload Whether the content must be downloded. If chrome cannot resolve a MIME
    *                     type, clicking a link will trigger a download. mustDownload is false if
    *                     the link has no download attribute and content-disposition is not
    *                     attachment.
    */
    void requestHttpGetDownload(DownloadInfo downloadInfo, boolean mustDownload);

    /**
     * Notify the host application that a download is started.
     * @param filename File name of the downloaded file.
     * @param mimeType Mime of the downloaded item.
     */
    void onDownloadStarted(String filename, String mimeType);

    /**
     * Notify the host application that a download has an extension indicating
     * a dangerous file type.
     * @param filename File name of the downloaded file.
     * @param downloadGuid The download GUID.
     */
    void onDangerousDownload(String filename, String downloadGuid);

    /**
     * Called when file access has been requested to complete a download.
     * @param callbackId The callback ID used to trigger success or failure of the download.
     *
     * @see DownloadController#onRequestFileAccessResult(long, boolean)
     */
    void requestFileAccess(long callbackId);
}
