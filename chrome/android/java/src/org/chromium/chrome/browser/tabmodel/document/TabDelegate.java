// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/**
 * Provides Tabs to a DocumentTabModel.
 */
public interface TabDelegate extends TabCreator {
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
      * Creates a new DocumentTab asynchronously.  Generally, you should use createNewTab() instead.
      * @param loadUrlParams parameters of the url load.
      * @param type Information about how the tab was launched.
      * @param parent the parent tab, if present.
      * @param documentLaunchMode Launch mode for the DocumentActivity.
      * @param documentStartedBy Reason that the DocumentActivity is being started.
      *        {@see DocumentMetricIds}.
      * @param requestId ServiceTabLauncher ID for the request.
      */
    void createNewDocumentTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent,
            int documentLaunchMode, int documentStartedBy, Integer requestId);
}