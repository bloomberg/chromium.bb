// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;

/**
 * {@link HistoryNavigationLayout#HistoryNavigationDelegate} implementation for
 * native pages in tabbed mode.
 */
public class HistoryNavigationDelegateImpl
        implements HistoryNavigationLayout.HistoryNavigationDelegate {
    private final Tab mTab;
    private final boolean mIsEnabled;
    private final boolean mDelegateSwipes;

    public HistoryNavigationDelegateImpl(Context context, Tab tab) {
        mTab = tab;
        mIsEnabled = ChromeFeatureList.isEnabled(ChromeFeatureList.OVERSCROLL_HISTORY_NAVIGATION)
                && (context instanceof ChromeActivity);
        mDelegateSwipes = ChromeFeatureList.isEnabled(ChromeFeatureList.DELEGATE_OVERSCROLL_SWIPES);
    }

    @Override
    public boolean isEnabled() {
        return mIsEnabled;
    }

    @Override
    public boolean delegateSwipes() {
        return mDelegateSwipes;
    }

    @Override
    public NavigationHandler.ActionDelegate createActionDelegate() {
        return new TabbedActionDelegate(mTab);
    }
}
