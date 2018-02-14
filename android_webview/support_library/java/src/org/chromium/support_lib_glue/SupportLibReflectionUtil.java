// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.support_lib_boundary.BoundaryInterfaceReflectionUtil;

import java.lang.reflect.InvocationHandler;

/**
 * Starting point for fetching WebView implementation.
 */
public class SupportLibReflectionUtil {
    /**
     * Entry point used by the WebView Support Library to gain access to the functionality in the
     * support library glue.
     * Changing the signature of this method will break existing WebView Support Library versions!
     */
    public static InvocationHandler createWebViewProviderFactory() {
        final SupportLibWebViewChromiumFactory factory = new SupportLibWebViewChromiumFactory();
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(factory);
    }
}
