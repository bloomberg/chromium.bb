// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.components.module_installer.ModuleInterface;

/**
 * Interface to get access to components concerning tab management.
 */
@ModuleInterface(module = "tab_management",
        impl = "org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegateImpl")
public interface TabManagementDelegate {
    /**
     * Create the {@link GridTabSwitcherLayout}.
     * @param context The current Android's context.
     * @param updateHost The parent {@link LayoutUpdateHost}.
     * @param renderHost The parent {@link LayoutRenderHost}.
     * @param gridTabSwitcher The {@link GridTabSwitcher} the layout should own.
     * @return The {@link GridTabSwitcherLayout}.
     */
    Layout createGTSLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, GridTabSwitcher gridTabSwitcher);
    GridTabSwitcher createGridTabSwitcher(ChromeActivity activity);
    TabGroupUi createTabGroupUi(ViewGroup parentView, ThemeColorProvider themeColorProvider);
}
