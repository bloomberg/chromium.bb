// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import java.io.File;

import android.content.Context;
import android.util.Log;
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
        // TODO(nileshagrawal): Implement.
        return null;
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
        // TODO(nileshagrawal): Implement.
    }

    /**
     * Notifies the DownloadListener that a new POST download has started.
     */
    @CalledByNative
    public void onHttpPostDownloadStarted(ContentView view) {
        // TODO(nileshagrawal): Implement.
    }

    /**
     * Notifies the DownloadListener that a POST download completed and passes along info about the
     * download.
     */
    @CalledByNative
    public void onHttpPostDownloadCompleted(ContentView view, String url,
            String contentDisposition, String mimetype, String path,
            long contentLength, boolean successful) {
        // TODO(nileshagrawal): Implement.
    }

    // native methods
    private native void nativeInit();
}
