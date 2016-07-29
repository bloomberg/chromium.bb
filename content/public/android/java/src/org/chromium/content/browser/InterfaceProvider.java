// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.mojo.bindings.Interface;
import org.chromium.mojo.bindings.Interface.Proxy;
import org.chromium.mojo.bindings.InterfaceRequest;

/**
 * Java wrapper around the shell service's InterfaceProvider type.
 */
@JNINamespace("content")
public class InterfaceProvider {

    <I extends Interface, P extends Proxy> void getInterface(
            Interface.Manager<I, P> manager, InterfaceRequest<I> request) {
        int nativeHandle = request.passHandle().releaseNativeHandle();
        nativeGetInterface(
                mNativeInterfaceProviderAndroid, manager.getName(), nativeHandle);
    }

    private long mNativeInterfaceProviderAndroid;

    private InterfaceProvider(long nativeInterfaceProviderAndroid) {
        mNativeInterfaceProviderAndroid = nativeInterfaceProviderAndroid;
    }

    @CalledByNative
    private static InterfaceProvider create(long nativeInterfaceProviderAndroid) {
        return new InterfaceProvider(nativeInterfaceProviderAndroid);
    }

    @CalledByNative
    private void destroy() {
        mNativeInterfaceProviderAndroid = 0;
    }

    private native void nativeGetInterface(long nativeInterfaceProviderAndroid, String name,
            int handle);
}
