// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.settings;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Settings is a Java wrapper to allow communicating with the native Settings object.
 */
@JNINamespace("blimp::client")
public class Settings {
    @CalledByNative
    private static Settings create(long nativeSettingsAndroid) {
        return new Settings(nativeSettingsAndroid);
    }

    private long mNativeSettingsAndroid;

    /**
     * The SettingsObserverProxy is lazily created when the first observer is added. It is used
     * instead of directly having an ObserverList in this class to ensure there is only a single
     * JNI hop for each call to observers.
     */
    private SettingsObserverProxy mObserverProxy;

    private Settings(long nativeSettingsAndroid) {
        mNativeSettingsAndroid = nativeSettingsAndroid;
    }

    public void addObserver(SettingsObserver observer) {
        assert mNativeSettingsAndroid != 0;
        if (mObserverProxy == null) mObserverProxy = new SettingsObserverProxy(this);
        mObserverProxy.addObserver(observer);
    }

    public void removeObserver(SettingsObserver observer) {
        if (mObserverProxy == null) return;
        mObserverProxy.removeObserver(observer);
    }

    /**
     * Set whether or not Blimp mode is enabled.
     */
    public void setEnableBlimpMode(boolean enable) {
        if (mNativeSettingsAndroid == 0) return;
        nativeSetEnableBlimpModeWrap(mNativeSettingsAndroid, enable);
    }

    /**
     * Set whether or not the engine sends the whole webpage at once or pieces of it based on the
     */
    public void setRecordWholeDocument(boolean enable) {
        if (mNativeSettingsAndroid == 0) return;
        nativeSetRecordWholeDocumentWrap(mNativeSettingsAndroid, enable);
    }

    /**
     *  Set whether or not to show the network stats.
     */
    public void setShowNetworkStats(boolean enable) {
        if (mNativeSettingsAndroid == 0) return;
        nativeSetShowNetworkStatsWrap(mNativeSettingsAndroid, enable);
    }

    /**
     * Clear the settings native pointer and destroy the ObserverProxy java object.
     */
    @CalledByNative
    private void onNativeDestroyed() {
        mNativeSettingsAndroid = 0;
        if (mObserverProxy != null) {
            mObserverProxy.destroy();
            mObserverProxy = null;
        }
    }

    @CalledByNative
    private long getNativePtr() {
        assert mNativeSettingsAndroid != 0;
        return mNativeSettingsAndroid;
    }

    private native void nativeSetEnableBlimpModeWrap(long nativeSettingsAndroid, boolean enable);
    private native void nativeSetRecordWholeDocumentWrap(
            long nativeSettingsAndroid, boolean enable);
    private native void nativeSetShowNetworkStatsWrap(long nativeSettingsAndroid, boolean enable);
}
