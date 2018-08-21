// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.framehost.RenderFrameHostImpl;
import org.chromium.content_public.browser.RenderFrameHost;

/**
 * The Java wrapper around RenderFrameHost to allow executing javascript for tests.
 */
@JNINamespace("content")
public class RenderFrameHostTestExt {
    private final long mNativeRenderFrameHost;

    public RenderFrameHostTestExt(RenderFrameHost host) {
        mNativeRenderFrameHost = ((RenderFrameHostImpl) host).getNativePtr();
    }

    /**
     * Runs the given JavaScript in the RenderFrameHost.
     *
     * @param script A String containing the JavaScript to run.
     * @param callback The Callback that will be called with the result of the JavaScript execution
     *        serialized to a String using JSONStringValueSerializer.
     */
    public void executeJavaScriptForTests(String script, Callback<String> callback) {
        nativeExecuteJavaScriptForTests(script, callback, mNativeRenderFrameHost);
    }

    private static native void nativeExecuteJavaScriptForTests(
            String script, Callback<String> callback, long nativeRenderFrameHost);
}
