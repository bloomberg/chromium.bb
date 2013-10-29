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
        // TODO(abarth): We should generate this list of libraries
        // automatically like content_shell does.
        System.loadLibrary("stlport_shared");
        System.loadLibrary("icuuc.cr");
        System.loadLibrary("icui18n.cr");
        System.loadLibrary("base.cr");
        System.loadLibrary("base_i18n.cr");
        System.loadLibrary("openssl.cr");
        System.loadLibrary("crcrypto.cr");
        System.loadLibrary("url_lib.cr");
        System.loadLibrary("net.cr");
        System.loadLibrary("mojo_system.cr");
        System.loadLibrary("mojo_shell.cr");
        Log.i(TAG, "libmojo_shell initialization success.");
    }
}
