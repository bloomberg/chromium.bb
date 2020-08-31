// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import androidx.annotation.VisibleForTesting;

/** Holds the {@link CachingUmaRecorder} used by {@link RecordHistogram}. */
public class UmaRecorderHolder {
    private UmaRecorderHolder() {}

    /** The instance held by this class. */
    private static CachingUmaRecorder sRecorder = new CachingUmaRecorder();

    /** Allow calling onLibraryLoaded() */
    private static boolean sAllowNativeUmaRecorder = true;

    /** Returns the held {@link UmaRecorder}. */
    public static UmaRecorder get() {
        return sRecorder;
    }

    /**
     * Set a new {@link UmaRecorder} delegate for the {@link CachingUmaRecorder}.
     * Returns after the cache has been flushed to the new delegate.
     * <p>
     * It should be used in processes that don't or haven't loaded native yet. This should never
     * be called after calling {@link #onLibraryLoaded()}.
     *
     * @param recorder the new UmaRecorder that metrics will be forwarded to.
     */
    public static void setNonNativeDelegate(UmaRecorder recorder) {
        UmaRecorder previous = sRecorder.setDelegate(recorder);
        assert !(previous instanceof NativeUmaRecorder)
            : "A NativeUmaRecorder has already been set";
    }

    /**
     * Disallow calling onLibraryLoaded() to set a {@link NativeUmaRecorder}.
     * <p>
     * Can be used in processes that don't load native library to make sure that a native recorder
     * is never set to avoid possible breakage.
     */
    public static void setAllowNativeUmaRecorder(boolean allow) {
        sAllowNativeUmaRecorder = allow;
    }

    /**
     * Starts forwarding metrics to the native code. Returns after the cache has been flushed.
     */
    public static void onLibraryLoaded() {
        assert sAllowNativeUmaRecorder : "Setting NativeUmaRecorder is not allowed";
        sRecorder.setDelegate(new NativeUmaRecorder());
    }

    /**
     * Tests may need to disable metrics. The value should be reset after the test done, to avoid
     * carrying over state to unrelated tests.
     *
     * @deprecated This method does nothing.
     */
    @VisibleForTesting
    @Deprecated
    public static void setDisabledForTests(boolean disabled) {}
}
