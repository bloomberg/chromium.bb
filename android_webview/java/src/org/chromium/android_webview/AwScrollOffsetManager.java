// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.CalledByNative;

class AwScrollOffsetManager {
    // The unit of all the values in this delegate are physical pixels
    public interface Delegate {
        // Returns whether the update succeeded
        boolean scrollContainerViewTo(int x, int y);
        void scrollNativeTo(int x, int y);
        int getContainerViewScrollX();
        int getContainerViewScrollY();
    }

    final Delegate mDelegate;
    int mNativeScrollX;
    int mNativeScrollY;

    public AwScrollOffsetManager(Delegate delegate) {
        mDelegate = delegate;
    }

    public void scrollContainerViewTo(int x, int y) {
        mNativeScrollX = x;
        mNativeScrollY = y;

        if (!mDelegate.scrollContainerViewTo(x, y)) {
            scrollNativeTo(mDelegate.getContainerViewScrollX(),
                    mDelegate.getContainerViewScrollY());
        }
    }

    public void onContainerViewScrollChanged(int x, int y) {
        scrollNativeTo(x, y);
    }

    private void scrollNativeTo(int x, int y) {
        if (x == mNativeScrollX && y == mNativeScrollY)
            return;

        mNativeScrollX = x;
        mNativeScrollY = y;

        mDelegate.scrollNativeTo(x, y);
    }
}
