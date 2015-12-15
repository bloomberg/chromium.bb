// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.library_loader;

/**
 * Loads native test libraries.
 */
public class TestLibraryLoader {

    public static void loadLibraries() {
        for (String library : NativeTestLibraries.LIBRARIES) {
            System.loadLibrary(library);
        }
    }

}
