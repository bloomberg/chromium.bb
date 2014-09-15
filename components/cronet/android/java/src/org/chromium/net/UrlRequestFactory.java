// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.util.concurrent.Executor;

/**
 * A factory for {@link UrlRequest}'s, which uses the best HTTP stack
 * available on the current platform.
 */
public interface UrlRequestFactory {
    /**
     * Creates an UrlRequest object. All UrlRequest functions must
     * be called on the Executor's thread, and all callbacks will be called
     * on the Executor's thread as well.
     * createRequest itself may be called on any thread.
     * @param url URL for the request.
     * @param listener Callback interface that gets called on different events.
     * @param executor Executor on which all callbacks will be called.
     * @return new request.
     */
    public abstract UrlRequest createRequest(String url,
            UrlRequestListener listener, Executor executor);
}
