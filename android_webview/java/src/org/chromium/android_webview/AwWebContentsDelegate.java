// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.content.components.web_contents_delegate_android.WebContentsDelegateAndroid;

/**
 * WebView-specific WebContentsDelegate.
 * This file is the Java version of the native class of the same name.
 * It should contain empty WebContentsDelegate methods to be implemented by the embedder.
 * These methods belong to WebView but are not shared with the Chromium Android port.
 */
@JNINamespace("android_webview")
public class AwWebContentsDelegate extends WebContentsDelegateAndroid {
    @CalledByNative
    public boolean addNewContents(boolean isDialog, boolean isUserGesture) {
        return false;
    }

    @CalledByNative
    public void closeContents() { }
}
