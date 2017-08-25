// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

/**
 * Used in {@link CronetBidirectionalStream}. Implements {@link NetworkExceptionImpl}.
 */
public class BidirectionalStreamNetworkException extends NetworkExceptionImpl {
    public BidirectionalStreamNetworkException(
            String message, int errorCode, int cronetInternalErrorCode) {
        super(message, errorCode, cronetInternalErrorCode);
    }

    @Override
    public boolean immediatelyRetryable() {
        switch (mErrorCode) {
            case ERROR_HTTP2_PING_FAILED:
            case ERROR_QUIC_HANDSHAKE_FAILED:
                return true;
            default:
                return super.immediatelyRetryable();
        }
    }
}
