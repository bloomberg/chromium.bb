// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import android.content.Context;

import org.chromium.base.VisibleForTesting;

import java.util.HashSet;
import java.util.Set;

/** Mock ModuleInstaller for use in tests. */
public class ModuleInstaller {
    private static Set<String> sModulesRequestedDeffered = new HashSet<>();

    public static void init() {}

    public static void initActivity(Context context) {}

    public static void recordModuleAvailability() {}

    public static void updateCrashKeys(){};

    public static void install(
            String moduleName, OnModuleInstallFinishedListener onFinishedListener) {}

    public static void installDeferred(String moduleName) {
        sModulesRequestedDeffered.add(moduleName);
    }

    @VisibleForTesting
    public static boolean didRequestDeferred(String moduleName) {
        return sModulesRequestedDeffered.contains(moduleName);
    }

    private ModuleInstaller() {}
}
