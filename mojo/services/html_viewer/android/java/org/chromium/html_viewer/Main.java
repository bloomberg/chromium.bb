// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.html_viewer;

import org.chromium.base.CalledByNative;
import org.chromium.base.PathUtils;

/**
 * This class does setup for html_viewer.
 */
public final class Main {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "html_viewer";

    private Main() {}

    @SuppressWarnings("unused")
    @CalledByNative
    private static void init() {
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
    }
}
