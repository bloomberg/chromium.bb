// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.asynctask;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.AsyncTask;

import java.util.concurrent.Executor;

/**
 * Forces async tasks to execute with the default executor.
 * This works around Robolectric not working out of the box with custom executors.
 *
 * @param <Params>
 * @param <Progress>
 * @param <Result>
 */
@Implements(AsyncTask.class)
public class CustomShadowAsyncTask<Params, Progress, Result>
        extends ShadowAsyncTask<Params, Progress, Result> {
    @SafeVarargs
    @Override
    @Implementation
    public final AsyncTask<Params, Progress, Result> executeOnExecutor(
            Executor executor, Params... params) {
        return super.execute(params);
    }
}
