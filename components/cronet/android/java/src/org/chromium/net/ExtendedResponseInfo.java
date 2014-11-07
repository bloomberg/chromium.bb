// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

/**
 * Contains additional information about a response. Sent to the embedder when
 * request is completed.
 */
public interface ExtendedResponseInfo {
    /**
     * Returns basic response info.
     */
    ResponseInfo getResponseInfo();

    /**
     * Returns the total amount of data received from network after SSL
     * decoding and proxy handling but before gzip and sdch decompression.
     * Available on request completion.
     */
    long getTotalReceivedBytes();
}
