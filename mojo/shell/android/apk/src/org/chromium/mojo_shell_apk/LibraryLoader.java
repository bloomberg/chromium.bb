// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo_shell_apk;

import android.util.Log;

public class LibraryLoader {
    private static final String TAG = "LibraryLoader";
    private static Boolean sInitialized = false;

    public static void ensureInitialized() throws UnsatisfiedLinkError {
        if (sInitialized)
            return;
        sInitialized = true;
        System.loadLibrary("mojo_shell.cr");
        Log.i(TAG, "libmojo_shell initialization success.");
    }
}
