// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.VisibleForTesting;

import java.io.InputStream;
import java.util.Map;

/**
 * The response information that is to be returned for a particular resource fetch.
 */
@JNINamespace("android_webview")
public class AwWebResourceResponse {
    private String mMimeType;
    private String mCharset;
    private InputStream mData;
    private int mStatusCode;
    private String mReasonPhrase;
    private String[] mResponseHeaderNames;
    private String[] mResponseHeaderValues;

    public AwWebResourceResponse(String mimeType, String encoding, InputStream data) {
        mMimeType = mimeType;
        mCharset = encoding;
        mData = data;
    }

    public AwWebResourceResponse(String mimeType, String encoding, InputStream data,
            int statusCode, String reasonPhrase, Map<String, String> responseHeaders) {
        this(mimeType, encoding, data);

        mStatusCode = statusCode;
        mReasonPhrase = reasonPhrase;

        if (responseHeaders != null) {
            mResponseHeaderNames = new String[responseHeaders.size()];
            mResponseHeaderValues = new String[responseHeaders.size()];
            int i = 0;
            for (Map.Entry<String, String> entry : responseHeaders.entrySet()) {
                mResponseHeaderNames[i] = entry.getKey();
                mResponseHeaderValues[i] = entry.getValue();
                i++;
            }
        }
    }

    @VisibleForTesting
    @CalledByNative
    public String getMimeType() {
        return mMimeType;
    }

    @CalledByNative
    private String getCharset() {
        return mCharset;
    }

    @VisibleForTesting
    @CalledByNative
    public InputStream getData() {
        return mData;
    }

    @CalledByNative
    private int getStatusCode() {
        return mStatusCode;
    }

    @CalledByNative
    private String getReasonPhrase() {
        return mReasonPhrase;
    }

    @CalledByNative
    private String[] getResponseHeaderNames() {
        return mResponseHeaderNames;
    }

    @CalledByNative
    private String[] getResponseHeaderValues() {
        return mResponseHeaderValues;
    }
}
