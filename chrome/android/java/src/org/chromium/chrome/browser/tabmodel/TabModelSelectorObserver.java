// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.Tab;

/**
 * Observes changes to the tab model selector.
 */
public interface TabModelSelectorObserver {
    /**
     * Called whenever the {@link TabModel} has changed.
     */
    void onChange();

    /**
     * Called when a new tab is created.
     */
    void onNewTabCreated(Tab tab);
}
