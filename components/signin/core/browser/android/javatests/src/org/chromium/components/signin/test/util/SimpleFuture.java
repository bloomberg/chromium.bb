// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import org.chromium.base.Callback;

/**
 * A simple tool to make waiting for the result of an asynchronous operation easy.
 *
 * This class is thread-safe; the result can be provided and retrieved on any thread.
 *
 * Example usage:
 *
 *    final SimpleFuture<Integer> future = new SimpleFuture<Integer>();
 *    getValueAsynchronously(new Callback<Integer>() {
 *        public void onResult(Integer result) {
 *            // Do some other work...
 *            future.provide(result);
 *        }
 *    }
 *    int value = future.get();
 *
 * Or, if your callback doesn't need to do anything but provide the value:
 *
 *    SimpleFuture<Integer> result = new SimpleFuture<Integer>();
 *    getValueAsynchronously(result.createCallback());
 *    int value = result.get();
 *
 * @param <V> The type of the value this future will return.
 */
public class SimpleFuture<V> {
    private static final int GET_TIMEOUT_MS = 10000;

    private final Object mLock = new Object();
    private boolean mHasResult = false;
    private V mResult;

    /**
     * Provide the result value of this future for get() to return.
     *
     * Any calls after the first are ignored.
     */
    public void provide(V result) {
        synchronized (mLock) {
            if (mHasResult) {
                // You can only provide a result once.
                return;
            }
            mHasResult = true;
            mResult = result;
            mLock.notifyAll();
        }
    }

    /**
     * Get the value of this future, or block until it's available.
     */
    public V get() throws InterruptedException {
        synchronized (mLock) {
            while (!mHasResult) {
                mLock.wait(GET_TIMEOUT_MS);
            }
            return mResult;
        }
    }

    /**
     * Helper function to create a {@link Callback} that will provide its result.
     */
    public Callback<V> createCallback() {
        return new Callback<V>() {
            @Override
            public void onResult(V result) {
                provide(result);
            }
        };
    }
}
