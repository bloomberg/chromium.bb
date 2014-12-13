// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_test_apk;

import android.os.ConditionVariable;
import android.util.Log;

import org.chromium.net.HttpUrlRequest;
import org.chromium.net.HttpUrlRequestListener;

import java.util.List;
import java.util.Map;

/**
 * A HttpUrlRequestListener that saves the response from a HttpUrlRequest.
 * This class is used in testing.
 */

public class TestHttpUrlRequestListener implements HttpUrlRequestListener {
    public static final String TAG = "TestHttpUrlRequestListener";

    public int mHttpStatusCode = 0;
    public String mHttpStatusText;
    public String mNegotiatedProtocol;
    public String mUrl;
    public byte[] mResponseAsBytes;
    public String mResponseAsString;
    public Exception mException;
    public Map<String, List<String>> mResponseHeaders;

    private final ConditionVariable mStarted = new ConditionVariable();
    private final ConditionVariable mComplete = new ConditionVariable();

    public TestHttpUrlRequestListener() {
    }

    @Override
    public void onResponseStarted(HttpUrlRequest request) {
        Log.i(TAG, "****** Response Started, content length is "
                + request.getContentLength());
        Log.i(TAG, "*** Headers Are *** " + request.getAllHeaders());
        mHttpStatusCode = request.getHttpStatusCode();
        mNegotiatedProtocol = request.getNegotiatedProtocol();
        mHttpStatusText = request.getHttpStatusText();
        mStarted.open();
    }

    @Override
    public void onRequestComplete(HttpUrlRequest request) {
        mUrl = request.getUrl();
        // mHttpStatusCode and mResponseHeaders are available in
        // onResponseStarted. However when redirects are disabled,
        // onResponseStarted is not invoked.
        Exception exception = request.getException();
        if (exception != null && exception.getMessage().equals("Request failed "
                + "because there were too many redirects or redirects have "
                + "been disabled")) {
            mHttpStatusCode = request.getHttpStatusCode();
            mResponseHeaders = request.getAllHeaders();
        }
        mResponseAsBytes = request.getResponseAsBytes();
        mResponseAsString = new String(mResponseAsBytes);
        mException = request.getException();
        mComplete.open();
        Log.i(TAG, "****** Request Complete over " + mNegotiatedProtocol
                + ", status code is " + mHttpStatusCode);
    }

    /**
     * Blocks until the response starts.
     */
    public void blockForStart() {
        mStarted.block();
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
