// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.tabmodel.document;

import android.app.Activity;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.document.PendingDocumentData;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegate;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel.Entry;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/**
 * Mocks out calls to get Tabs for the DocumentTabModel.
 */
public class MockTabDelegate implements TabDelegate {
    @Override
    public Tab getActivityTab(boolean incognito, ActivityDelegate delgate, Activity activity) {
        return null;
    }

    @Override
    public void createTabInForeground(Activity parentActivity, boolean incognito,
            LoadUrlParams loadUrlParams, PendingDocumentData documentParams) {
    }

    @Override
    public Tab createFrozenTab(Entry entry) {
        return null;
    }

    @Override
    public void createTabWithWebContents(boolean isIncognito, WebContents webContents,
            int parentTabId) {
    }

    @Override
    public void createTabForDevTools(String url) {
    }
}