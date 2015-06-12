// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.tabmodel.document;

import android.app.Activity;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegate;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/**
 * Mocks out calls to get Tabs for the DocumentTabModel.
 */
public class MockTabDelegate implements TabDelegate {
    @Override
    public Tab getActivityTab(ActivityDelegate delgate, Activity activity) {
        return null;
    }

    @Override
    public Tab createNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent) {
        return null;
    }

    @Override
    public Tab createFrozenTab(TabState state, int id, int index) {
        return null;
    }

    @Override
    public Tab createTabWithWebContents(WebContents webContents, int parentId, TabLaunchType type) {
        return null;
    }

    @Override
    public Tab createTabWithWebContents(
            WebContents webContents, int parentId, TabLaunchType type, int startedBy) {
        return null;
    }

    @Override
    public Tab launchUrl(String url, TabLaunchType type) {
        return null;
    }

    @Override
    public Tab launchNTP() {
        return null;
    }
}