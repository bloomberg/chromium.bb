// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.support.annotation.Nullable;

import org.chromium.components.module_installer.ModuleInstaller;

/**
 * Provider class for TabManagementModule.
 */
public class TabManagementModuleProvider {
    private static final String TAB_MANAGEMENT_MODULE_IMPL_CLASS_NAME =
            "org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleImpl";

    /**
     * Returns fallback or real {@link TabManagementModule} implementation depending on whether
     * the module is installed.
     */
    public static @Nullable TabManagementModule getTabManagementModule() {
        TabManagementModule tabManagementModule;
        try {
            tabManagementModule =
                    (TabManagementModule) Class.forName(TAB_MANAGEMENT_MODULE_IMPL_CLASS_NAME)
                            .newInstance();
        } catch (ClassNotFoundException | InstantiationException | IllegalAccessException
                | IllegalArgumentException e) {
            ModuleInstaller.installDeferred("tab_ui");
            return null;
        }
        return tabManagementModule;
    }
}
