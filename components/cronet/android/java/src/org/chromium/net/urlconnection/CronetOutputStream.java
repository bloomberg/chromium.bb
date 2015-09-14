// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import org.chromium.net.UploadDataProvider;

import java.io.IOException;
import java.io.OutputStream;

/**
 * An abstract class of {@link OutputStream} that concrete implementations must
 * extend in order to be used in {@link CronetHttpURLConnection}.
 */
abstract class CronetOutputStream extends OutputStream {
    /**
     * Tells the underlying implementation that connection has been established.
     * Used in {@link CronetHttpURLConnection}.
     */
    abstract void setConnected() throws IOException;

    /**
     * Checks whether content received is less than Content-Length.
     * Used in {@link CronetHttpURLConnection}.
     */
    abstract void checkReceivedEnoughContent() throws IOException;

    /**
     * Returns {@link UploadDataProvider} implementation.
     */
    abstract UploadDataProvider getUploadDataProvider();
}
