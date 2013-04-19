// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.CalledByNative;
import org.chromium.content.browser.ContentView;
import org.chromium.ui.WindowAndroid;

/**
 * The basic Java representation of a tab.  Contains and manages a {@link ContentView}.
 *
 * TabBase provides common functionality for ChromiumTestshell's Tab as well as Chrome on Android's
 * tab. It's intended to be extended both on Java and C++, with ownership managed by the subclass.
 * Because of the inner-workings of JNI, the subclass is responsible for constructing the native
 * subclass which in turn constructs TabAndroid (the native counterpart to TabBase) which in turn
 * sets the native pointer for TabBase. The same is true for destruction. The Java subclass must be
 * destroyed which will call into the native subclass and finally lead to the destruction of the
 * parent classes.
 */
public abstract class TabBase {
    private final WindowAndroid mWindowAndroid;
    private int mNativeTabAndroid;

    protected TabBase(WindowAndroid window) {
        mWindowAndroid = window;
    }

    @CalledByNative
    private void destroyBase() {
        assert mNativeTabAndroid != 0;
        mNativeTabAndroid = 0;
    }

    @CalledByNative
    private void setNativePtr(int nativePtr) {
        assert mNativeTabAndroid == 0;
        mNativeTabAndroid = nativePtr;
    }

    protected WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }
}
