// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.res.Resources;

import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ColorUtils;

import javax.inject.Inject;

/**
 * Manages the status bar color for a CustomTabActivity.
 */
@ActivityScope
public class CustomTabStatusBarColorProvider {
    private final Resources mResources;
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final CustomTabActivityTabProvider mCustomTabActivityTabProvider;

    @Inject
    public CustomTabStatusBarColorProvider(Resources resources,
            CustomTabIntentDataProvider intentDataProvider,
            CustomTabActivityTabProvider customTabActivityTabProvider) {
        mResources = resources;
        mIntentDataProvider = intentDataProvider;
        mCustomTabActivityTabProvider = customTabActivityTabProvider;
    }

    int getBaseStatusBarColor(int fallbackStatusBarColor) {
        if (mIntentDataProvider.isOpenedByChrome()) return fallbackStatusBarColor;
        Tab tab = mCustomTabActivityTabProvider.getTab();
        if (tab!= null && tab.isPreview()) {
            return ColorUtils.getDefaultThemeColor(mResources, false);
        }
        return mIntentDataProvider.getToolbarColor();
    }

    boolean isStatusBarDefaultThemeColor(boolean isFallbackColorDefault) {
        if (mIntentDataProvider.isOpenedByChrome()) return isFallbackColorDefault;
        return false;
    }
}
