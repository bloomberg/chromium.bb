// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Java-side bridge that receives and responds to native-initiated requests for debugging data. This
 * data is then used to populate the Feed Internals page.
 */
@JNINamespace("feed")
public class FeedDebuggingBridge {
    @CalledByNative
    static String getFeedFetchUrl() {
        return FeedConfiguration.getFeedServerEndpoint();
    }
}
