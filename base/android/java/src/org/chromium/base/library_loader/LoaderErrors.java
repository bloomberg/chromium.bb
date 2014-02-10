// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

/**
 * These are the possible failures from the LibraryLoader
 */
public class LoaderErrors {
    // TODO(aberent) These were originally in content/common/result_codes_list.h
    // although they are never used on the c++ side, or in places where they
    // could be confused with other result codes. A copy must remain there, and the
    // values must be identical, until the downstream patch to use this new class
    // lands.
    public static final int LOADER_ERROR_NORMAL_COMPLETION = 0;
    public static final int LOADER_ERROR_FAILED_TO_REGISTER_JNI = 4;
    public static final int LOADER_ERROR_NATIVE_LIBRARY_LOAD_FAILED = 5;
    public static final int LOADER_ERROR_NATIVE_LIBRARY_WRONG_VERSION = 6;
    public static final int LOADER_ERROR_NATIVE_STARTUP_FAILED = 7;
}
