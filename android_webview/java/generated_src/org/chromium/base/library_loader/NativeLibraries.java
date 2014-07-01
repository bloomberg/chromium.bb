// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

/**
 * This class defines the native libraries and loader options required by webview
 */
public class NativeLibraries {
    // Set to true to use the chromium linker. Only useful to save memory
    // on multi-process content-based projects. Always disabled for the Android Webview.
    public static boolean USE_LINKER = false;

    // Set to true to directly load the library from the zip file using the
    // chromium linker. Always disabled for Android Webview.
    public static boolean USE_LIBRARY_IN_ZIP_FILE = false;

    // Set to true to enable chromium linker test support. NEVER enable this for the
    // Android webview.
    public static boolean ENABLE_LINKER_TESTS = false;

    // This is the list of native libraries to load. In the normal chromium build, this would be
    // automatically generated.
    // TODO(torne, cjhopman): Use a generated file for this.
    static final String[] LIBRARIES = { "webviewchromium" };
    // This should match the version name string returned by the native library.
    // TODO(aberent) The Webview native library currently returns an empty string; change this
    // to a string generated at compile time, and incorporate that string in a generated
    // replacement for this file.
    static String VERSION_NUMBER = "";
}
