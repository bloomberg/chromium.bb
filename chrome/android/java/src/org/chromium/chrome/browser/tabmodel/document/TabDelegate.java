// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.app.Activity;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.content_public.browser.WebContents;

/**
 * Provides Tabs to a DocumentTabModel.
 */
public interface TabDelegate extends TabCreator {
    /**
     * Returns the Tab for the given Activity.
     * @param delgate Stores information about DocumentActivities.
     * @param activity Activity to grab the Tab of.
     * @return Tab for the DocumentActivity, if it is a valid DocumentActivity.  Null otherwise.
     */
    Tab getActivityTab(ActivityDelegate activityDelegate, Activity activity);

    /**
     * Creates a Tab to host the given WebContents.
     * @param webContents WebContents that has been pre-created.
     * @param parentId ID of the parent Tab.
     * @param type Launch type for the Tab.
     * @param startedBy See {@link DocumentMetricIds}.
     */
    Tab createTabWithWebContents(
            WebContents webContents, int parentId, TabLaunchType type, int startedBy);
}