// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core;

import android.content.Context;

import org.chromium.blimp_public.BlimpClientContextDelegate;

/**
 * Mock {@link BlimpClientContextDelegate}, to test embedder delegate functions.
 * This test support class only tests Java layer and is not backed by any native code.
 */
public class MockBlimpClientContextDelegate implements BlimpClientContextDelegate {
    int mRestartCalled = 0;
    int mStartUserSignInFlowCalled = 0;

    public void reset() {
        mRestartCalled = 0;
        mStartUserSignInFlowCalled = 0;
    }

    public int restartBrowserCalled() {
        return mRestartCalled;
    }

    public int startUserSignInFlowCalled() {
        return mStartUserSignInFlowCalled;
    }

    @Override
    public void restartBrowser() {
        mRestartCalled++;
    }

    @Override
    public void startUserSignInFlow(Context context) {
        mStartUserSignInFlowCalled++;
    }
}
