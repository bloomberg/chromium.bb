// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.settings;

import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Serves as a compound observer proxy for dispatching SettingsObserver callbacks, avoiding
 * redundant JNI-related work when there are multiple Java-based observers.
 */
@JNINamespace("blimp::client")
class SettingsObserverProxy implements SettingsObserver {
    private final ObserverList<SettingsObserver> mObservers = new ObserverList<>();

    private long mNativeSettingsObserverProxy;

    SettingsObserverProxy(Settings settings) {
        mNativeSettingsObserverProxy = nativeInit(settings);
    }

    void destroy() {
        if (mNativeSettingsObserverProxy != 0) {
            nativeDestroy(mNativeSettingsObserverProxy);
            mNativeSettingsObserverProxy = 0;
        }
        mObservers.clear();
    }

    void addObserver(SettingsObserver observer) {
        assert mNativeSettingsObserverProxy != 0;
        mObservers.addObserver(observer);
    }

    void removeObserver(SettingsObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    @CalledByNative
    public void onBlimpModeEnabled(boolean enable) {
        for (SettingsObserver observer : mObservers) {
            observer.onBlimpModeEnabled(enable);
        }
    }

    @Override
    @CalledByNative
    public void onShowNetworkStatsChanged(boolean enable) {
        for (SettingsObserver observer : mObservers) {
            observer.onShowNetworkStatsChanged(enable);
        }
    }

    @Override
    @CalledByNative
    public void onRecordWholeDocumentChanged(boolean enable) {
        for (SettingsObserver observer : mObservers) {
            observer.onRecordWholeDocumentChanged(enable);
        }
    }

    @Override
    @CalledByNative
    public void onRestartRequired() {
        for (SettingsObserver observer : mObservers) {
            observer.onRestartRequired();
        }
    }

    private native long nativeInit(Settings settings);
    private native void nativeDestroy(long nativeSettingsObserverProxy);
}
