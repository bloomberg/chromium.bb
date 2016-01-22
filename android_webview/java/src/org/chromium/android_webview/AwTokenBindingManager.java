// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.Uri;
import android.webkit.ValueCallback;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.security.KeyPair;
import java.security.spec.AlgorithmParameterSpec;

/**
 * AwTokenBindingManager manages the token binding protocol.
 *
 * see https://tools.ietf.org/html/draft-ietf-tokbind-protocol-03
 *
 * The token binding protocol can be enabled for browser context
 * separately. However all webviews share the same browser context and do not
 * expose the browser context to the embedder app. As such, there is no way to
 * enable token binding manager for individual webviews.
 */
@JNINamespace("android_webview")
public final class AwTokenBindingManager {
    public void enableTokenBinding() {
        nativeEnableTokenBinding();
    }

    public void getKey(Uri origin, AlgorithmParameterSpec[] spec, ValueCallback<KeyPair> callback) {
        if (callback == null) {
            throw new IllegalArgumentException("callback can't be null");
        }
        nativeGetTokenBindingKey(origin.getHost(), callback);
    }

    public void deleteKey(Uri origin, ValueCallback<Boolean> callback) {
        if (origin == null) {
            throw new IllegalArgumentException("origin can't be null");
        }
        // null callback is allowed
        nativeDeleteTokenBindingKey(origin.getHost(), callback);
    }

    public void deleteAllKeys(ValueCallback<Boolean> callback) {
        // null callback is allowed
        nativeDeleteAllTokenBindingKeys(callback);
    }

    @CalledByNative
    private static void onKeyReady(ValueCallback<KeyPair> callback) {
        callback.onReceiveValue(null);
    }

    @CalledByNative
    private static void onDeletionComplete(ValueCallback<Boolean> callback) {
        // At present, the native deletion complete callback always succeeds.
        callback.onReceiveValue(true);
    }

    private native void nativeEnableTokenBinding();
    private native void nativeGetTokenBindingKey(String host, ValueCallback<KeyPair> callback);
    private native void nativeDeleteTokenBindingKey(String host, ValueCallback<Boolean> callback);
    private native void nativeDeleteAllTokenBindingKeys(ValueCallback<Boolean> callback);
}
