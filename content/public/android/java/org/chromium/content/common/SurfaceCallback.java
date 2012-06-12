// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

import android.view.Surface;

/* This implements the entry point for passing a Surface handle received through Binder
 * back to the native code.
 */
public class SurfaceCallback {
    // Calling setSurface passes ownership to the callee and calls release() on the passed in
    // object.
    public static void setSurface(int type, Surface surface, int primaryID, int secondaryID) {
        nativeSetSurface(type, surface, primaryID, secondaryID);
    }

    /**
     * Sets up the Surface iBinder for a producer identified by the IDs.
     *
     * @param type The install type for the Surface
     * @param surface The parceled Surface to set.
     * @param primaryID Used to identify the correct target instance.
     * @param secondaryID Used to identify the correct target instance.
     */
    private static native void nativeSetSurface(int type, Surface surface,
            int primaryID, int secondaryID);
}
