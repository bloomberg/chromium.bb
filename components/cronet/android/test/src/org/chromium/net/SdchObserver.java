// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.ConditionVariable;

import junit.framework.Assert;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Class to watch for Sdch dictionary events. The native implementation
 * unregisters itself when an event happens. Therefore, an instance of this
 * class is only able to receive a notification of the earliest event.
 */
@JNINamespace("cronet")
public class SdchObserver {
    private static final int BLOCK_WAIT_TIMEOUT_SEC = 20;
    private final ConditionVariable mAddBlock = new ConditionVariable();

    /**
     * Constructor.
     * @param targetUrl the target url on which sdch encoding will be used.
     * @param contextAdapter the native context adapter to register the observer.
     */
    public SdchObserver(String targetUrl, long contextAdapter) {
        nativeAddSdchObserver(targetUrl, contextAdapter);
    }

    /**
     * Called when a dictionary is added to the SdchManager for the target url.
     */
    @CalledByNative
    protected void onDictionaryAdded() {
        mAddBlock.open();
    }

    /**
     * Called after the observer has been registered.
     */
    @CalledByNative
    protected void onAddSdchObserverCompleted() {
        // Left blank;
    }

    /**
     * Called if the dictionary was added before the observer registration.
     */
    @CalledByNative
    protected void onDictionarySetAlreadyPresent() {
        mAddBlock.open();
    }

    public void waitForDictionaryAdded() {
        boolean success = mAddBlock.block(BLOCK_WAIT_TIMEOUT_SEC * 1000);
        if (!success) {
            Assert.fail("Timeout: the dictionary hasn't been added after waiting for "
                    + BLOCK_WAIT_TIMEOUT_SEC + " seconds");
        }
    }

    private native void nativeAddSdchObserver(String targetUrl, long contextAdapter);
}
