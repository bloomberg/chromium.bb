// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;

import org.chromium.content.browser.ContentViewCore;

/**
 * This class is responsible for calling certain client callbacks on the UI thread.
 *
 * Most callbacks do no go through here, but get forwarded to AwContentsClient directly. The
 * messages processed here may originate from the IO or UI thread.
 */
class AwContentsClientCallbackHelper {

    private static class DownloadInfo {
        final String mUrl;
        final String mUserAgent;
        final String mContentDisposition;
        final String mMimeType;
        final long mContentLength;

        DownloadInfo(String url,
                     String userAgent,
                     String contentDisposition,
                     String mimeType,
                     long contentLength) {
            mUrl = url;
            mUserAgent = userAgent;
            mContentDisposition = contentDisposition;
            mMimeType = mimeType;
            mContentLength = contentLength;
        }
    }

    private final static int MSG_ON_LOAD_RESOURCE = 1;
    private final static int MSG_ON_PAGE_STARTED = 2;
    private final static int MSG_ON_DOWNLOAD_START = 3;

    private final AwContentsClient mContentsClient;

    private final Handler mHandler = new Handler(Looper.getMainLooper()) {
        @Override
        public void handleMessage(Message msg) {
            switch(msg.what) {
                case MSG_ON_LOAD_RESOURCE: {
                    final String url = (String) msg.obj;
                    mContentsClient.onLoadResource(url);
                    break;
                }
                case MSG_ON_PAGE_STARTED: {
                    final String url = (String) msg.obj;
                    mContentsClient.onPageStarted(url);
                    break;
                }
                case MSG_ON_DOWNLOAD_START: {
                    DownloadInfo info = (DownloadInfo) msg.obj;
                    mContentsClient.onDownloadStart(info.mUrl, info.mUserAgent,
                            info.mContentDisposition, info.mMimeType, info.mContentLength);
                    break;
                }
                default:
                    throw new IllegalStateException(
                            "AwContentsClientCallbackHelper: unhandled message " + msg.what);
            }
        }
    };

    public AwContentsClientCallbackHelper(AwContentsClient contentsClient) {
        mContentsClient = contentsClient;
    }

    public void postOnLoadResource(String url) {
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_LOAD_RESOURCE, url));
    }

    public void postOnPageStarted(String url) {
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_PAGE_STARTED, url));
    }

    public void postOnDownloadStart(String url, String userAgent, String contentDisposition,
            String mimeType, long contentLength) {
        DownloadInfo info = new DownloadInfo(url, userAgent, contentDisposition, mimeType,
                contentLength);
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_DOWNLOAD_START, info));
    }
}
