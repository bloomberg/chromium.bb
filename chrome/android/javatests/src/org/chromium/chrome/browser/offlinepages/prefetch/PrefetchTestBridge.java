// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;

/** Prefetch test Java to native bridge. */
@JNINamespace("offline_pages::prefetch")
public class PrefetchTestBridge {
    public static void addSuggestedURLs(Profile profile, String[] urls) {
        nativeAddSuggestedURLs(profile, urls);
    }
    public static void enableLimitlessPrefetching(boolean enabled) {
        nativeEnableLimitlessPrefetching(enabled);
    }
    public static boolean isLimitlessPrefetchingEnabled() {
        return nativeIsLimitlessPrefetchingEnabled();
    }

    static native void nativeAddSuggestedURLs(Profile profile, String[] urls);
    static native void nativeEnableLimitlessPrefetching(boolean enabled);
    static native boolean nativeIsLimitlessPrefetchingEnabled();
}
