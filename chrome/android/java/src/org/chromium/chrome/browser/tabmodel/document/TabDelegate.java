// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.app.Activity;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.document.PendingDocumentData;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel.Entry;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Provides Tabs to a DocumentTabModel.
 */
public interface TabDelegate {
    /**
     * Returns the Tab for the given Activity.
     * @param incognito Whether the Activity is supposed to hold an incognito Tab.
     * @param delgate Sotres information about DocumentActivities.
     * @param activity Activity to grab the Tab of.
     * @return Tab for the DocumentActivity, if it is a valid DocumentActivity.  Null otherwise.
     */
    Tab getActivityTab(boolean incognito, ActivityDelegate delgate, Activity activity);

    /**
     * Opens a new Tab in the foreground.
     * Assumed to be triggered by a window.open().
     */
    void createTabInForeground(Activity parentActivity, boolean incognito,
            LoadUrlParams loadUrlParams, PendingDocumentData documentParams);

    /**
     * Creates a frozen Tab for the Entry.
     * @param entry Entry containing TabState.
     * @return A frozen Tab.
     */
    Tab createFrozenTab(Entry entry);

    /**
     * Creates a new Activity for the pre-created WebContents.
     * @param isIncognito Whether the Activity is supposed to hold an incognito Tab.
     * @param webContentsPtr Native-side WebContents pointer to use for the new Tab.
     * @param parentTabId ID of the spawning Tab.
     */
    void createTabWithNativeContents(boolean isIncognito, long webContentsPtr, int parentTabId);

    /**
     * Creates a new Tab for a URL typed into DevTools.
     * @param url URL to spawn a Tab for.
     */
    void createTabForDevTools(String url);

    /**
     * Check if the tab is covered by its child activity.
     * @param tab Tab to be checked.
     * @return Whether the tab is covered by its child activity.
     */
    boolean isTabCoveredByChildActivity(Tab tab);
}