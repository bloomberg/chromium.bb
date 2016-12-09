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
    public void resume() {
        mGvrLayout.getGvrApi().resumeTracking();
    }

    @Override
    public void pause() {
        // We can't pause/resume the GvrLayout, because doing so will force us to enter VR. However,
        // we should be safe not pausing it as we never add it to the view hierarchy, or give it a
        // presentation view, so there's nothing to pause but the tracking.
        mGvrLayout.getGvrApi().pauseTracking();
    }

    @Override
    public void shutdown() {
        mGvrLayout.shutdown();
    }
}
