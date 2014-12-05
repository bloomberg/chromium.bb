// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

/**
 * An InputStream that is used by {@link CronetHttpURLConnection} to request
 * data from the network stack as needed.
 */
class CronetInputStream extends InputStream {
    private final CronetHttpURLConnection mHttpURLConnection;
    // Indicates whether listener's onSucceeded or onFailed callback is invoked.
    private boolean mResponseDataCompleted;
    private ByteBuffer mBuffer;

    /**
     * Constructs a CronetInputStream.
     * @param httpURLConnection the CronetHttpURLConnection that is associated
     *            with this InputStream.
     */
    public CronetInputStream(CronetHttpURLConnection httpURLConnection) {
        mHttpURLConnection = httpURLConnection;
    }

    @Override
    public int read() throws IOException {
        if (!mResponseDataCompleted
                && (mBuffer == null || !mBuffer.hasRemaining())) {
            // Requests more data from CronetHttpURLConnection.
            mBuffer = mHttpURLConnection.getMoreData();
        }
        if (mBuffer != null && mBuffer.hasRemaining()) {
            return mBuffer.get() & 0xFF;
        }
        return -1;
    }

    void setResponseDataCompleted() {
        mResponseDataCompleted = true;
    }
}
