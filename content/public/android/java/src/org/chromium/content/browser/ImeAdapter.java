// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.content.app.AppResource;

@JNINamespace("content")
class ImeAdapter {

    private int mNativeImeAdapterAndroid;
    private int mTextInputType;

    @CalledByNative
    void detach() {
        mNativeImeAdapterAndroid = 0;
        mTextInputType = 0;
    }

    boolean unselect() {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        nativeUnselect(mNativeImeAdapterAndroid);
        return true;
    }

    boolean selectAll() {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        nativeSelectAll(mNativeImeAdapterAndroid);
        return true;
    }

    boolean cut() {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        nativeCut(mNativeImeAdapterAndroid);
        return true;
    }

    boolean copy() {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        nativeCopy(mNativeImeAdapterAndroid);
        return true;
    }

    boolean paste() {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        nativePaste(mNativeImeAdapterAndroid);
        return true;
    }

    private native void nativeUnselect(int nativeImeAdapterAndroid);
    private native void nativeSelectAll(int nativeImeAdapterAndroid);
    private native void nativeCut(int nativeImeAdapterAndroid);
    private native void nativeCopy(int nativeImeAdapterAndroid);
    private native void nativePaste(int nativeImeAdapterAndroid);
}