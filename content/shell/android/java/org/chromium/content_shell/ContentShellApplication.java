// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import android.app.Application;

/**
 * Entry point for the content shell application.  Handles initialization of information that needs
 * to be shared across the main activity and the sandbox services created.
 */
public class ContentShellApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        // TODO(tedchoc): Initialize the .pak files to load and the native library name.
    }

}
