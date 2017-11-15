// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import java.lang.reflect.InvocationHandler;

/**
 */
public interface WebViewProviderFactory {
    // android.webkit.WebViewProvider is in the SystemApi, not public, so we pass it as an Object
    // here.
    InvocationHandler /* org.chromium.sup_lib_boundary.WebViewProvider */ createWebView(
            Object /* android.webkit.WebViewProvider */ webviewProvider);
}
