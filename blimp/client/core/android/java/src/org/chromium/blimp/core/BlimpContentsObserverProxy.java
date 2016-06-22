// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core;

import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.core_public.BlimpContentsObserver;

/**
 * Serves as a compound observer proxy for dispatching BlimpContentsObserver callbacks,
 * avoiding redundant JNI-related work when there are multiple Java-based observers.
 */
@JNINamespace("blimp::client")
class BlimpContentsObserverProxy implements BlimpContentsObserver {
    private final ObserverList<BlimpContentsObserver> mObservers = new ObserverList<>();

    private long mNativeBlimpContentsObserverProxy;

    BlimpContentsObserverProxy(BlimpContentsImpl blimpContentsImpl) {
        mNativeBlimpContentsObserverProxy = nativeInit(blimpContentsImpl);
    }

    void destroy() {
        if (mNativeBlimpContentsObserverProxy != 0) {
            nativeDestroy(mNativeBlimpContentsObserverProxy);
            mNativeBlimpContentsObserverProxy = 0;
        }
        mObservers.clear();
    }

    void addObserver(BlimpContentsObserver observer) {
        assert mNativeBlimpContentsObserverProxy != 0;
        mObservers.addObserver(observer);
    }

    void removeObserver(BlimpContentsObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    @CalledByNative
    public void onUrlUpdated(String url) {
        for (BlimpContentsObserver observer : mObservers) {
            observer.onUrlUpdated(url);
        }
    }

    private native long nativeInit(BlimpContentsImpl blimpContentsImpl);
    private native void nativeDestroy(long nativeBlimpContentsObserverProxy);
}
