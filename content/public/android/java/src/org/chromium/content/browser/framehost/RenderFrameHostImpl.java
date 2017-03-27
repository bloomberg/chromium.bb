// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.framehost;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.RenderFrameHost;

/**
 * The RenderFrameHostImpl Java wrapper to allow communicating with the native RenderFrameHost
 * object.
 */
@JNINamespace("content")
public class RenderFrameHostImpl implements RenderFrameHost {
    private long mNativeRenderFrameHostAndroid;
    // mDelegate can be null.
    final RenderFrameHostDelegate mDelegate;
    final boolean mIncognito;

    private RenderFrameHostImpl(long nativeRenderFrameHostAndroid, RenderFrameHostDelegate delegate,
            boolean isIncognito) {
        mNativeRenderFrameHostAndroid = nativeRenderFrameHostAndroid;
        mDelegate = delegate;
        mIncognito = isIncognito;
    }

    @CalledByNative
    private static RenderFrameHostImpl create(long nativeRenderFrameHostAndroid,
            RenderFrameHostDelegate delegate, boolean isIncognito) {
        return new RenderFrameHostImpl(nativeRenderFrameHostAndroid, delegate, isIncognito);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeRenderFrameHostAndroid = 0;
    }

    /**
     * Get the delegate associated with this RenderFrameHost.
     *
     * @return The delegate associated with this RenderFrameHost.
     */
    public RenderFrameHostDelegate getRenderFrameHostDelegate() {
        return mDelegate;
    }

    @Override
    public String getLastCommittedURL() {
        if (mNativeRenderFrameHostAndroid == 0) return null;
        return nativeGetLastCommittedURL(mNativeRenderFrameHostAndroid);
    }

    /**
     * Returns whether we're in incognito mode.
     *
     * TODO(timloh): This function shouldn't really be on here. If we end up
     * needing more logic from the native BrowserContext, we should add a
     * wrapper for that and move this function there.
     */
    public boolean isIncognito() {
        return mIncognito;
    }

    private native String nativeGetLastCommittedURL(long nativeRenderFrameHostAndroid);
}
