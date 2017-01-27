// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.app.Activity;

import com.google.vr.ndk.base.GvrLayout;

/**
 * Creates an active GvrContext from a detached GvrLayout. This is used by magic window mode.
 */
public class NonPresentingGvrContextImpl implements NonPresentingGvrContext {
    private GvrLayout mGvrLayout;

    public NonPresentingGvrContextImpl(Activity activity) {
        mGvrLayout = new GvrLayout(activity);
    }

    @Override
    public long getNativeGvrContext() {
        return mGvrLayout.getGvrApi().getNativeGvrContext();
    }

    @Override
    public void shutdown() {
        mGvrLayout.shutdown();
    }
}
