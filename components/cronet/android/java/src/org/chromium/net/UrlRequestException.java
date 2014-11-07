// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.io.IOException;

/**
 * Exception after UrlRequest start. Could be reported by network stack, in
 * which case netError() will contain native error code.
 */
public class UrlRequestException extends IOException {
    /** Net error code if exception is reported by native. */
    final int mNetError;

    UrlRequestException(String message, Throwable cause) {
        super(message, cause);
        mNetError = 0;
    }

    UrlRequestException(String message, int netError) {
        super(message, null);
        mNetError = netError;
    }

    /** @return Error code if exception is reported by native. */
    public int netError() {
        return mNetError;
    }
}
