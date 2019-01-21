// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

/** Dummy fallback of ModuleInstaller for APK builds. */
public class ModuleInstaller {
    public static void init() {}
    public static void updateCrashKeys(){};

    /** Should never be called for APK builds. */
    public static void install(
            String moduleName, OnModuleInstallFinishedListener onFinishedListener) {
        throw new UnsupportedOperationException("Cannot install module if APK");
    }

    private ModuleInstaller() {}
}
