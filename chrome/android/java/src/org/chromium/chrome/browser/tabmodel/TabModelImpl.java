// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;

/**
 * This class contains additions to the base TabModel implementation (notifications
 * and overrides of existing methods).
 *
 * TODO: merge this into TabModelBase after upstreaming.
 */
public class TabModelImpl extends TabModelBase {
    public TabModelImpl(boolean incognito,
            ChromeActivity activity, TabModelSelectorUma uma,
            TabModelOrderController orderController, TabContentManager tabContentManager,
            TabPersistentStore tabSaver, TabModelDelegate modelDelegate) {
        super(incognito, activity, uma, orderController, tabContentManager, tabSaver,
                modelDelegate);
    }
}
