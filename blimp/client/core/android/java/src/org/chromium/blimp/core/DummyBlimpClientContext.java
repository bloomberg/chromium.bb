// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core;

import android.preference.PreferenceFragment;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp_public.BlimpClientContext;
import org.chromium.blimp_public.BlimpClientContextDelegate;
import org.chromium.blimp_public.contents.BlimpContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashMap;
import java.util.Map;

/**
 * A dummy implementation of the {@link BlimpClientContext}.
 */
@JNINamespace("blimp::client")
public class DummyBlimpClientContext implements BlimpClientContext {
    @CalledByNative
    private static DummyBlimpClientContext create(long nativeDummyBlimpClientContextAndroid) {
        return new DummyBlimpClientContext(nativeDummyBlimpClientContextAndroid);
    }

    /**
     * The pointer to the DummyBlimpClientContextAndroid JNI bridge.
     */
    private long mNativeDummyBlimpClientContextAndroid;

    private DummyBlimpClientContext(long nativeDummyBlimpClientContextAndroid) {
        mNativeDummyBlimpClientContextAndroid = nativeDummyBlimpClientContextAndroid;
    }

    @Override
    public BlimpContents createBlimpContents(WindowAndroid windowAndroid) {
        return null;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeDummyBlimpClientContextAndroid = 0;
    }

    @CalledByNative
    private long getNativePtr() {
        assert mNativeDummyBlimpClientContextAndroid != 0;
        return mNativeDummyBlimpClientContextAndroid;
    }

    @Override
    public boolean isBlimpSupported() {
        return false;
    }

    @Override
    public boolean isBlimpEnabled() {
        return false;
    }

    @Override
    public void attachBlimpPreferences(PreferenceFragment fragment) {}

    @Override
    public void setDelegate(BlimpClientContextDelegate delegate) {}

    @Override
    public void connect() {}

    @Override
    public Map<String, String> getFeedbackMap() {
        return new HashMap<>();
    }
}
