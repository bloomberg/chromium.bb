// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.graphics.SurfaceTexture;
import android.view.Surface;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

@JNINamespace("content")
class BrowserProcessSurfaceTexture {
    /**
     * Called from native code to set up the peer surface texture producer in
     * another process.
     *
     * @param pid Process handle of the sandboxed process to share the
     *            SurfaceTexture with.
     * @param type The type of process that the SurfaceTexture is for.
     * @param st The SurfaceTexture object to share with the sandboxed process.
     * @param primaryID Used to route the call to the correct client instance.
     * @param secondaryID Used to route the call to the correct client instance.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    static void establishSurfaceTexturePeer(int pid, int type, SurfaceTexture st, int primaryID,
            int secondaryID) {
        Surface surface = new Surface(st);
        SandboxedProcessLauncher.establishSurfacePeer(pid, type, surface, primaryID, secondaryID);

        // We need to explicitly release the native resource of our newly created surface
        // or the Surface class will print a warning message to the log in its finalizer.
        // This should be ok to do since our caller is responsible for retaining a
        // reference to the SurfaceTexture that is being sent across processes and the
        // receiving end should have retained a reference before the binder call finished.
        surface.release();
    }
}
