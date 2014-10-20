// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_test_apk;

import android.os.ConditionVariable;
import android.util.Log;

import org.chromium.net.HttpUrlRequest;
import org.chromium.net.HttpUrlRequestListener;

/**
 * A HttpUrlRequestListener that saves the response from a HttpUrlRequest.
 * This class is used in testing.
 */

public class TestHttpUrlRequestListener implements HttpUrlRequestListener {
    public static final String TAG = "TestHttpUrlRequestListener";

    public int mHttpStatusCode = 0;
    public String mNegotiatedProtocol;

    public String mUrl;
    public byte[] mResponseAsBytes;
    public String mResponseAsString;
    public Exception mException;

    private ConditionVariable mComplete = new ConditionVariable();

    public TestHttpUrlRequestListener() {
    }

    @Override
    public void onResponseStarted(HttpUrlRequest request) {
        Log.i(TAG, "****** Response Started, content length is " +
                request.getContentLength());
        Log.i(TAG, "*** Headers Are *** " + request.getAllHeaders());
        mHttpStatusCode = request.getHttpStatusCode();
        mNegotiatedProtocol = request.getNegotiatedProtocol();
    }

    @Override
    public void onRequestComplete(HttpUrlRequest request) {
        mUrl = request.getUrl();
        mResponseAsBytes = request.getResponseAsBytes();
        mResponseAsString = new String(mResponseAsBytes);
        mException = request.getException();
        mComplete.open();
        Log.i(TAG, "****** Request Complete, status code is " +
                request.getHttpStatusCode());
    }

    /**
     * Blocks until the request completes.
     */
    public void blockForComplete() {
        mComplete.block();
    }

    public void resetComplete() {
        mComplete.close();
    }
}
