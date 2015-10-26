// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

/**
 * A simple single-argument callback to handle the result of a computation.
 *
 * @param <T> The type of the computation's result.
 */
public interface Callback<T> {
    /**
     * Invoked with the result of a computation.
     */
    public void onResult(T result);
}
