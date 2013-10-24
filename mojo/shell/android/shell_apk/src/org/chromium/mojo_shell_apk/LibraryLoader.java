// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo_shell_apk;

import java.lang.UnsatisfiedLinkError;
import org.chromium.base.JNINamespace;

@JNINamespace("mojo")
public class LibraryLoader {
    private static Boolean sInitialized = false;

    public static void ensureInitialized() throws UnsatisfiedLinkError {
        if (!sInitialized)
            return;
        sInitialized = true;
        System.loadLibrary("mojo_shell");
    }
}
