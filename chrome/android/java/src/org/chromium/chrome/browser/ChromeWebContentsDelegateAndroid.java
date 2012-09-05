// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.component.web_contents_delegate_android.WebContentsDelegateAndroid;

/**
 * Chromium Android specific WebContentsDelegate.
 * This file is the Java version of the native class of the same name.
 * It should contain empty WebContentsDelegate methods to be implemented by the embedder.
 * These methods belong to the Chromium Android port but not to WebView.
 */
public class ChromeWebContentsDelegateAndroid extends WebContentsDelegateAndroid {

    @CalledByNative
    public void onFindResultAvailable(FindNotificationDetails result) {
    }

    @CalledByNative
    public void onFindMatchRectsAvailable(FindMatchRectsDetails result) {
    }
}
