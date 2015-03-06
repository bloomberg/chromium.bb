// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import org.chromium.base.JNINamespace;

/**
 * Used for recording metrics about WebappActivity instances.
 */
@JNINamespace("webapps")
public class WebappMetrics {
    /**
     * Records the creation of a WebappActivity.
     * @param url URL that kicked off the Activity's creation.
     */
    public static void recordActivityStart(String url) {
        nativeRecordActivityStart(url);
    }

    private static native void nativeRecordActivityStart(String url);
}
