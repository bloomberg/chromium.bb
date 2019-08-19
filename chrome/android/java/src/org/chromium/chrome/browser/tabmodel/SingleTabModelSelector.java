// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;

import org.chromium.chrome.browser.tab.Tab;

/**
 * Simple TabModelSelector that assumes that only a single TabModel exists at a time.
 */
public class SingleTabModelSelector extends TabModelSelectorBase {
    public SingleTabModelSelector(Activity activity, TabCreatorManager tabCreatorManager,
            boolean incognito, boolean blockNewWindows) {
        super(tabCreatorManager);
        initialize(incognito, new SingleTabModel(activity, incognito, blockNewWindows));
    }

    public void setTab(Tab tab) {
        ((SingleTabModel) getCurrentModel()).setTab(tab);
        markTabStateInitialized();
    }
}
