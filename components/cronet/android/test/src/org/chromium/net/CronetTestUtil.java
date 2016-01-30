// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.chromium.base.annotations.JNINamespace;

/**
 * Utilities for Cronet testing
 */
@JNINamespace("cronet")
public class CronetTestUtil {
    /**
     * Start QUIC server on local host.
     * @return non-zero QUIC server port number on success or 0 if failed.
     */
    public static int startQuicServer() {
        return nativeStartQuicServer();
    }

    private static native int nativeStartQuicServer();
}
