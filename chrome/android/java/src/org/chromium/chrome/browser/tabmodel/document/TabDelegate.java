// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.app.Activity;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.document.PendingDocumentData;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel.Entry;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

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
     * Creates a frozen Tab for the Entry.  This Tab is not meant to be used or unfrozen -- it is
     * only used as a placeholder until the real Tab can be created.
     * @param entry Entry containing TabState.
     * @return A frozen Tab.
     */
    Tab createFrozenTab(Entry entry);

    /**
     * Creates a new Activity for the pre-created WebContents.
     * @param isIncognito Whether the Activity is supposed to hold an incognito Tab.
     * @param webContents WebContents to use with the new Tab.
     * @param parentTabId ID of the spawning Tab.
     */
    void createTabWithWebContents(boolean isIncognito, WebContents webContents, int parentTabId);

    /**
     * Creates a new Tab for a URL typed into DevTools.
     * @param url URL to spawn a Tab for.
     */
    void createTabForDevTools(String url);
}