// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import android.app.Application;
import org.chromium.content.browser.LibraryLoader;

/**
 * Entry point for the content shell application.  Handles initialization of information that needs
 * to be shared across the main activity and the sandbox services created.
 */
public class ContentShellApplication extends Application {

    // TODO(jrg): do not downstream this filename!
    private static final String NATIVE_LIBRARY = "content_shell_content_view";

    @Override
    public void onCreate() {
        super.onCreate();
        // TODO(tedchoc): Initialize the .pak files to load
        LibraryLoader.setLibraryToLoad(NATIVE_LIBRARY);
    }

}
