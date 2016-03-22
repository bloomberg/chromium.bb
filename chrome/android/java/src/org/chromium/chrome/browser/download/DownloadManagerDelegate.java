// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.DownloadManager;
import android.content.Context;

/**
 * A wrapper for Android DownloadManager to provide utility functions.
 */
public class DownloadManagerDelegate {
    /**
     * @see android.app.DownloadManager#addCompletedDownload(String, String, boolean, String,
     * String, long, boolean)
     */
    protected long addCompletedDownload(Context context, String fileName, String description,
            String mimeType, String path, long length, String originalUrl, String referer) {
        DownloadManager manager =
                (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
        return manager.addCompletedDownload(fileName, description, true, mimeType, path, length,
                false);
    }
}
