// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.ViewGroup;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.components.module_installer.ModuleInterface;

/**
 * Interface to get access to components concerning tab management.
 */
@ModuleInterface(module = "tab_management",
        impl = "org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegateImpl")
public interface TabManagementDelegate {
    GridTabSwitcher createGridTabSwitcher(ChromeActivity activity);
    TabGroupUi createTabGroupUi(ViewGroup parentView, ThemeColorProvider themeColorProvider);
}
