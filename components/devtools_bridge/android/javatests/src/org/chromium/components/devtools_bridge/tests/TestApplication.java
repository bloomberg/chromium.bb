// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.tests;

import android.app.Application;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.components.devtools_bridge.SessionDependencyFactory;
import org.chromium.components.devtools_bridge.SessionDependencyFactoryNative;

/**
 * Performs initialization for DevTools Bridge test APK.
 */
public class TestApplication extends Application {
    static {
        System.loadLibrary("devtools_bridge_natives_so");
        SessionDependencyFactory.init(SessionDependencyFactoryNative.class);
        try {
            LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized();
        } catch (ProcessInitException e) {
            throw new RuntimeException(e);
        }
    }
}
