// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabState;

/**
 * An interface to return a {@link TabCreator} either for regular or incognito tabs.
 */
public interface TabCreatorManager {
    /**
     * Creates Tabs.
     */
    public interface TabCreator {
        /**
         * On restore, allows us to create a frozen version of a tab using saved tab state we read
         * from disk.
         * @param state    The tab state that the tab can be restored from.
         * @param id       The id to give the new tab.
         * @param index    The index for where to place the tab.
         */
        public void createFrozenTab(TabState state, int id, int index);

        /**
         * Creates a new tab and loads the NTP.
         * @return The created tab.
         */
        public Tab launchNTP();
    }

    /**
     * @return A {@link TabCreator} that will create either regular or incognito tabs.
     * @param incognito True if the method should return the TabCreator for incognito tabs, false
     *                  for regular tabs.
     */
    TabCreator getTabCreator(boolean incognito);
}
