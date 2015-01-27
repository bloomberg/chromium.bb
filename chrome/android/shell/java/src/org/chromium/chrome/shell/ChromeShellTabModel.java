// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelBase;
import org.chromium.chrome.browser.tabmodel.TabModelDelegate;
import org.chromium.chrome.browser.tabmodel.TabModelOrderController;
import org.chromium.content_public.browser.WebContents;

/**
 * Basic implementation of TabModel for use in ChromeShell.
 */
public class ChromeShellTabModel extends TabModelBase {

    public ChromeShellTabModel(TabModelOrderController orderController,
            TabModelDelegate modelDelegate) {
        super(false, orderController, modelDelegate);
    }

    @Override
    protected Tab createTabWithWebContents(boolean incognito, WebContents webContents,
            int parentId) {
        return null;
    }

    @Override
    protected Tab createNewTabForDevTools(String url) {
        assert false;
        return null;
    }

}
