// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core;

import android.preference.PreferenceFragment;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.core.settings.AboutBlimpPreferences;
import org.chromium.blimp.core.settings.PreferencesUtil;
import org.chromium.blimp_public.BlimpClientContext;
import org.chromium.blimp_public.BlimpClientContextDelegate;
import org.chromium.blimp_public.BlimpSettingsCallbacks;
import org.chromium.blimp_public.contents.BlimpContents;

/**
 * BlimpClientContextImpl is a Java wrapper to allow communicating with the native
 * BlimpClientContextImpl object.
 */
@JNINamespace("blimp::client")
public class BlimpClientContextImpl implements BlimpClientContext {

    // Delegate that contains functions Blimp needed in the embedder.
    private BlimpClientContextDelegate mDelegate;

    /**
     * Get embedder delegate which provides necessary functionality and callbacks.
     *
     * The delegate is created through JNI in early startup.
     *
     * @return BlimpClientContextDelegate, which contains functions we need in embedder.
     */
    public BlimpClientContextDelegate getDelegate() {
        return mDelegate;
    }

    @Override
    public void setDelegate(BlimpClientContextDelegate delegate) {
        mDelegate = delegate;
    }

    @CalledByNative
    private static BlimpClientContextImpl create(long nativeBlimpClientContextImplAndroid) {
        return new BlimpClientContextImpl(nativeBlimpClientContextImplAndroid);
    }

    /**
     * The pointer to the BlimpClientContextImplAndroid JNI bridge.
     */
    private long mNativeBlimpClientContextImplAndroid;

    private BlimpClientContextImpl(long nativeBlimpClientContextImplAndroid) {
        mNativeBlimpClientContextImplAndroid = nativeBlimpClientContextImplAndroid;
    }

    @Override
    public BlimpContents createBlimpContents() {
        assert mNativeBlimpClientContextImplAndroid != 0;
        return nativeCreateBlimpContentsJava(mNativeBlimpClientContextImplAndroid);
    }

    @Override
    public boolean isBlimpSupported() {
        return true;
    }

    @Override
    public boolean isBlimpEnabled() {
        return false;
    }

    @Override
    public void attachBlimpPreferences(
            PreferenceFragment fragment, BlimpSettingsCallbacks callbacks) {
        AboutBlimpPreferences.addBlimpPreferences(fragment);
        AboutBlimpPreferences.registerCallback(callbacks);
    }

    @Override
    public void connect() {
        assert mNativeBlimpClientContextImplAndroid != 0;
        nativeConnectFromJava(mNativeBlimpClientContextImplAndroid);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeBlimpClientContextImplAndroid = 0;
    }

    @CalledByNative
    private long getNativePtr() {
        assert mNativeBlimpClientContextImplAndroid != 0;
        return mNativeBlimpClientContextImplAndroid;
    }

    @CalledByNative
    private String getAssignerUrl() {
        return PreferencesUtil.getLastUsedAssigner();
    }

    private native BlimpContents nativeCreateBlimpContentsJava(
            long nativeBlimpClientContextImplAndroid);

    private native void nativeConnectFromJava(long nativeBlimpClientContextImplAndroid);
}
