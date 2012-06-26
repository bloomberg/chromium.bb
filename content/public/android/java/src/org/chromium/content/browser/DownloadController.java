// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.webkit.DownloadListener;

import org.chromium.base.CalledByNative;

/**
 * Java counterpart of android DownloadController.
 *
 * Its a singleton class instantiated by the C++ DownloadController.
 */
class DownloadController {
    private static final String LOGTAG = "DownloadController";
    private static DownloadController sInstance;

    public static DownloadController getInstance() {
        if (sInstance == null) {
            sInstance = new DownloadController();
        }
        return sInstance;
    }

    private Context mContext;

    private DownloadController() {
        nativeInit();
    }

    private static DownloadListener listenerFromView(ContentView view) {
        return view.downloadListener();
    }

    private static ContentViewDownloadDelegate downloadDelegateFromView(ContentView view) {
        return view.getDownloadDelegate();
    }

    public void setContext(Context context) {
        mContext = context;
    }

    /**
     * Notifies the DownloadListener of a new GET download and passes all the information
     * needed to download the file.
     *
     * The DownloadListener is expected to handle the download.
     */
    @CalledByNative
    public void newHttpGetDownload(ContentView view, String url,
            String userAgent, String contentDisposition, String mimetype,
            String cookie, long contentLength) {
        ContentViewDownloadDelegate downloadDelagate = downloadDelegateFromView(view);

        if (downloadDelagate != null) {
            downloadDelagate.requestHttpGetDownload(url, userAgent,
                    contentDisposition, mimetype, cookie, contentLength);
            return;
        }

        DownloadListener listener = listenerFromView(view);
        if (listener != null) {
            listener.onDownloadStart(url, userAgent, contentDisposition,
                    mimetype, contentLength);
        }
    }

    /**
     * Notifies the DownloadListener that a new POST download has started.
     */
    @CalledByNative
    public void onHttpPostDownloadStarted(ContentView view) {
        ContentViewDownloadDelegate downloadDelagate = downloadDelegateFromView(view);

        if (downloadDelagate != null) {
            downloadDelagate.onHttpPostDownloadStarted();
        }
    }

    /**
     * Notifies the DownloadListener that a POST download completed and passes along info about the
     * download.
     */
    @CalledByNative
    public void onHttpPostDownloadCompleted(ContentView view, String url,
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
