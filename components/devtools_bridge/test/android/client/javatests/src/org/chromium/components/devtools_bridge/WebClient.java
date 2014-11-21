// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import org.chromium.base.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Java wrapper over native WebClient for tests.
 */
@JNINamespace("devtools_bridge::android")
public final class WebClient {
    private final long mWebClientPtr;

    public WebClient(Profile profile) {
        mWebClientPtr = nativeCreateWebClient(profile);
    }

    public void dispose() {
        nativeDestroyWebClient(mWebClientPtr);
    }

    private static native long nativeCreateWebClient(Profile profile);
    private static native void nativeDestroyWebClient(long webClientPtr);
}
