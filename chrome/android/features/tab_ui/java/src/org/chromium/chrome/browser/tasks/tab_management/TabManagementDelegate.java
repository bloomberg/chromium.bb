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
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.components.module_installer.ModuleInterface;

/**
 * Interface to get access to components concerning tab management.
 * TODO(crbug.com/982018): Move DFM configurations to 'chrome/android/modules/start_surface/'
 */
@ModuleInterface(module = "tab_management",
        impl = "org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegateImpl")
public interface TabManagementDelegate {
    /**
     * Create the {@link GridTabSwitcher}.
     * @param activity The {@link ChromeActivity} creates this switcher.
     * @return The {@link GridTabSwitcher}.
     */
    GridTabSwitcher createGridTabSwitcher(ChromeActivity activity);

    /**
     * Create the {@link TabGroupUi}.
     * @param parentView The parent view of this UI.
     * @param themeColorProvider The {@link ThemeColorProvider} for this UI.
     * @return The {@link TabGroupUi}.
     */
    TabGroupUi createTabGroupUi(ViewGroup parentView, ThemeColorProvider themeColorProvider);

    /**
     * Create the {@link StartSurfaceLayout}.
     * @param context The current Android's context.
     * @param updateHost The parent {@link LayoutUpdateHost}.
     * @param renderHost The parent {@link LayoutRenderHost}.
     * @param startSurface The {@link StartSurface} the layout should own.
     * @return The {@link StartSurfaceLayout}.
     */
    Layout createStartSurfaceLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, StartSurface startSurface);

    /**
     * Create the {@link StartSurface}
     * @param activity The {@link ChromeActivity} creates this {@link StartSurface}.
     * @return the {@link StartSurface}
     */
    StartSurface createStartSurface(ChromeActivity activity);
}
