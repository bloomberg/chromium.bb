// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.util.concurrent.Executor;

/**
 * Listens for finished requests for the purpose of collecting metrics.
 *
 * {@hide} as it's a prototype.
 */
public abstract class RequestFinishedListener {
    private final Executor mExecutor;

    public RequestFinishedListener(Executor executor) {
        if (executor == null) {
            throw new IllegalStateException("Executor must not be null");
        }
        mExecutor = executor;
    }

    /**
     * Invoked with request info. Will be called in a task submitted to the
     * {@link java.util.concurrent.Executor} returned by {@link #getExecutor}.
     * @param requestInfo {@link CronetEngine#UrlRequestInfo} for finished request.
     */
    public abstract void onRequestFinished(CronetEngine.UrlRequestInfo requestInfo);

    /**
     * Returns this listener's executor. Can be called on any thread.
     * @return this listener's {@link java.util.concurrent.Executor}
     */
    public Executor getExecutor() {
        return mExecutor;
    }
}
