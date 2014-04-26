// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.util.Log;

/**
 * Cronet Library Loader.
 */
public class LibraryLoader {
    private static final String TAG = "LibraryLoader";

    private static Boolean sInitialized = false;

    public static void ensureInitialized() throws UnsatisfiedLinkError {
        if (sInitialized)
            return;
        sInitialized = true;
        System.loadLibrary("cronet");
        Log.i(TAG, "libcronet initialization success.");
    }
}
