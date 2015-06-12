// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/**
 * An interface to return a {@link TabCreator} either for regular or incognito tabs.
 */
public interface TabCreatorManager {
    /**
     * Creates Tabs.
     */
    public interface TabCreator {
        /**
         * Creates a new tab and posts to UI.
         * @param loadUrlParams parameters of the url load.
         * @param type Information about how the tab was launched.
         * @param parent the parent tab, if present.
         * @return The new tab.
         */
        Tab createNewTab(LoadUrlParams loadUrlParams, TabModel.TabLaunchType type, Tab parent);

        /**
         * On restore, allows us to create a frozen version of a tab using saved tab state we read
         * from disk.
         * @param state    The tab state that the tab can be restored from.
         * @param id       The id to give the new tab.
         * @param index    The index for where to place the tab.
         */
        Tab createFrozenTab(TabState state, int id, int index);

        /**
         * Creates a tab around the native web contents pointer.
         * @param webContents The web contents to create a tab around.
         * @param parentId    The id of the parent tab.
         * @param type        The TabLaunchType describing how this tab was created.
         * @return            The created tab.
         */
        Tab createTabWithWebContents(WebContents webContents, int parentId, TabLaunchType type);

        /**
         * Creates a new tab and loads the NTP.
         * @return The created tab.
         */
        Tab launchNTP();

        /**
         * Creates a new tab and loads the specified URL in it. This is a convenience method for
         * {@link #createNewTab} with the default {@link LoadUrlParams} and no parent tab.
         *
         * @param url the URL to open.
         * @param type the type of action that triggered that launch. Determines how the tab is
         *             opened (for example, in the foreground or background).
         * @return the created tab.
         */
        Tab launchUrl(String url, TabModel.TabLaunchType type);
    }

    /**
     * @return A {@link TabCreator} that will create either regular or incognito tabs.
     * @param incognito True if the method should return the TabCreator for incognito tabs, false
     *                  for regular tabs.
     */
    TabCreator getTabCreator(boolean incognito);
}
