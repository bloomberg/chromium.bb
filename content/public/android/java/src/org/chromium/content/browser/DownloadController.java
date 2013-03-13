// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * Java counterpart of android DownloadController.
 *
 * Its a singleton class instantiated by the C++ DownloadController.
 */
@JNINamespace("content")
class DownloadController {
    private static final String LOGTAG = "DownloadController";
    private static DownloadController sInstance;

    @CalledByNative
    public static DownloadController getInstance() {
        if (sInstance == null) {
            sInstance = new DownloadController();
        }
        return sInstance;
    }

    private DownloadController() {
        nativeInit();
    }

    private static ContentViewDownloadDelegate downloadDelegateFromView(ContentViewCore view) {
        return view.getDownloadDelegate();
    }

    /**
     * Notifies the download delegate of a new GET download and passes all the information
     * needed to download the file.
     *
     * The download delegate is expected to handle the download.
     */
    @CalledByNative
    public void newHttpGetDownload(ContentViewCore view, String url,
            String userAgent, String contentDisposition, String mimetype,
            String cookie, String referer, long contentLength) {
        ContentViewDownloadDelegate downloadDelagate = downloadDelegateFromView(view);

        if (downloadDelagate != null) {
            downloadDelagate.requestHttpGetDownload(url, userAgent, contentDisposition,
                    mimetype, cookie, referer, contentLength);
        }
    }

    /**
     * Notifies the download delegate that a new POST download has started.
     */
    @CalledByNative
    public void onHttpPostDownloadStarted(ContentViewCore view) {
        ContentViewDownloadDelegate downloadDelagate = downloadDelegateFromView(view);

        if (downloadDelagate != null) {
            downloadDelagate.onHttpPostDownloadStarted();
        }
    }

    /**
     * Notifies the download delegate that a POST download completed and passes along info about the
     * download.
     */
    @CalledByNative
    public void onHttpPostDownloadCompleted(ContentViewCore view, String url,
            String contentDisposition, String mimetype, String path,
            long contentLength, boolean successful) {
        ContentViewDownloadDelegate downloadDelagate = downloadDelegateFromView(view);

        if (downloadDelagate != null) {
            downloadDelagate.onHttpPostDownloadCompleted(
                    url, mimetype, path, contentLength, successful);
        }
    }

    // native methods
    private native void nativeInit();
}
