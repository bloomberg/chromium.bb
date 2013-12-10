// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.unittest;

import org.chromium.base.CalledByNative;

import java.io.InputStream;

class InputStreamUnittest {
    private InputStreamUnittest() {
    }

    @CalledByNative
    static InputStream getEmptyStream() {
        return new InputStream() {
            @Override
            public int read() {
                return -1;
            }
        };
    }

    @CalledByNative
    static InputStream getCountingStream(final int size) {
        return new InputStream() {
            private int count = 0;

            @Override
            public int read() {
                if (count < size)
                    return count++ % 256;
                else
                    return -1;
            }
        };
    }
}
