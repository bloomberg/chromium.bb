// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.util.concurrent.Executor;

/**
 * A factory for {@link AsyncUrlRequest}'s, which uses the best HTTP stack
 * available on the current platform.
 */
public abstract class AsyncUrlRequestFactory {
    /**
     * Creates an AsyncUrlRequest object. All AsyncUrlRequest functions must
     * be called on the Executor's thread, and all callbacks will be called
     * on the Executor's thread as well.
     * createAsyncRequest itself may be called on any thread.
     * @param url URL for the request.
     * @param listener Callback interface that gets called on different events.
     * @param executor Executor on which all callbacks will be called.
     * @return new request.
     */
    public abstract AsyncUrlRequest createAsyncRequest(String url,
            AsyncUrlRequestListener listener, Executor executor);
}
