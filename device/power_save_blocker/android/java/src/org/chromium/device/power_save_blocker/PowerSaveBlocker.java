// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.power_save_blocker;

import android.view.View;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.lang.ref.WeakReference;

@JNINamespace("device")
class PowerSaveBlocker {
    // WeakReference to prevent leaks in Android WebView.
    private WeakReference<View> mKeepScreenOnView;

    @CalledByNative
    private static PowerSaveBlocker create() {
        return new PowerSaveBlocker();
    }

    private PowerSaveBlocker() {}

    @CalledByNative
    private void applyBlock(View anchorView) {
        assert mKeepScreenOnView == null;
        mKeepScreenOnView = new WeakReference<>(anchorView);
        anchorView.setKeepScreenOn(true);
    }

    @CalledByNative
    private void removeBlock() {
        // mKeepScreenOnView may be null since it's possible that |applyBlock()|
        // was not invoked due to having failed to acquire an anchor view.
        if (mKeepScreenOnView == null) return;
        View anchorView = mKeepScreenOnView.get();
        mKeepScreenOnView = null;
        if (anchorView == null) return;

        anchorView.setKeepScreenOn(false);
    }
}
