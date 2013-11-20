// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import android.text.TextUtils;

import org.chromium.chrome.browser.TabBase;
import org.chromium.printing.Printable;

import java.lang.ref.WeakReference;

/**
 * Wraps printing related functionality of a {@link TabBase} object.
 *
 * This class doesn't have any lifetime expectations with regards to Tab, since we keep a weak
 * reference.
 */
public class TabPrinter implements Printable {
    private static String sDefaultTitle;

    private final WeakReference<TabBase> mTab;

    public TabPrinter(TabBase tab) {
        mTab = new WeakReference<TabBase>(tab);
    }

    public static void setDefaultTitle(String defaultTitle) {
        sDefaultTitle = defaultTitle;
    }

    @Override
    public boolean print() {
        TabBase tab = mTab.get();
        return tab != null && tab.print();
    }

    @Override
    public String getTitle() {
        TabBase tab = mTab.get();
        if (tab == null) return sDefaultTitle;

        String title = tab.getTitle();
        if (!TextUtils.isEmpty(title)) return title;

        String url = tab.getUrl();
        if (!TextUtils.isEmpty(url)) return url;

        return sDefaultTitle;
    }
}
