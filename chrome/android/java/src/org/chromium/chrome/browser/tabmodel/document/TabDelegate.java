// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.document.DocumentMetricIds;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

import javax.annotation.Nullable;

/**
 * Provides Tabs to a DocumentTabModel.
 */
public interface TabDelegate extends TabCreator {
    /** Optional parameters for creating a Tab asynchronously. */
    public static final class AsyncTabCreationParams {
        /** How to start a document mode Tab. */
        public int documentLaunchMode = ChromeLauncherActivity.LAUNCH_MODE_FOREGROUND;

        /** What caused a document mode Tab to be created. */
        public int documentStartedBy = DocumentMetricIds.STARTED_BY_UNKNOWN;

        /** Used by the ServiceTabLauncher. */
        public Integer requestId;
    }

    /**
     * Creates a Tab to host the given WebContents asynchronously.
     * @param webContents WebContents that has been pre-created.
     * @param parentId ID of the parent Tab.
     * @param type Launch type for the Tab.
     * @param url URL that the WebContents was opened for.
     * @param startedBy See {@link DocumentMetricIds}.
     */
    void createTabWithWebContents(
            WebContents webContents, int parentId, TabLaunchType type, String url, int startedBy);

     /**
     * Creates a Tab to host the given WebContents asynchronously.
      * @param loadUrlParams      Parameters of the url load.
      * @param type               Information about how the tab was launched.
      * @param parent             The parent tab, if it exists.
      * @param additionalParams   Additional parameters to load.
      */
    void createNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, @Nullable Tab parent,
            AsyncTabCreationParams additionalParams);
}