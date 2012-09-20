// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;
import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

@JNINamespace("android_webview")
public class JsResultHandler implements JsResultReceiver, JsPromptResultReceiver {
    private int mNativeDialogPointer;

    @CalledByNative
    public static JsResultHandler create(int nativeDialogPointer) {
        return new JsResultHandler(nativeDialogPointer);
    }

    private JsResultHandler(int nativeDialogPointer) {
        mNativeDialogPointer = nativeDialogPointer;
    }

    @Override
    public void confirm() {
        confirm(null);
    }

    @Override
    public void confirm(String promptResult) {
        nativeConfirmJsResult(mNativeDialogPointer, promptResult);
    }

    @Override
    public void cancel() {
        nativeCancelJsResult(mNativeDialogPointer);
    }

    private native void nativeConfirmJsResult(int dialogPointer, String promptResult);
    private native void nativeCancelJsResult(int dialogPointer);
}
