// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.power_save_blocker;

import android.view.View;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.base.ViewAndroidDelegate;

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
    private void applyBlock(ViewAndroidDelegate delegate) {
        assert mKeepScreenOnView == null;
        View anchorView = delegate.acquireAnchorView();
        mKeepScreenOnView = new WeakReference<>(anchorView);
        delegate.setAnchorViewPosition(anchorView, 0, 0, 0, 0);
        anchorView.setKeepScreenOn(true);
    }

    @CalledByNative
    private void removeBlock(ViewAndroidDelegate delegate) {
        assert mKeepScreenOnView != null;
        View anchorView = mKeepScreenOnView.get();
        mKeepScreenOnView = null;
        if (anchorView == null) return;

        anchorView.setKeepScreenOn(false);
        delegate.releaseAnchorView(anchorView);
    }
}
