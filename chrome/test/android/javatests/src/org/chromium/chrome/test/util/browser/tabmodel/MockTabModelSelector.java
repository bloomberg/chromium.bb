// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.tabmodel;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Mock of a basic {@link TabModelSelector}. It supports 2 {@link TabModel}: standard and incognito.
 */
public class MockTabModelSelector extends TabModelSelectorBase {
    // Offsetting the id compared to the index helps greatly when debugging.
    public static final int ID_OFFSET = 1000;
    public static final int INCOGNITO_ID_OFFSET = 2000;

    public MockTabModelSelector(
            int tabCount, int incognitoTabCount, MockTabModel.MockTabModelDelegate delegate) {
        super();
        MockTabModel tabModel = new MockTabModel(false, delegate);
        if (tabCount > 0) {
            for (int i = 0; i < tabCount; i++) {
                tabModel.addTab(ID_OFFSET + i);
            }
            TabModelUtils.setIndex(tabModel, 0);
        }

        MockTabModel incognitoTabModel = new MockTabModel(true, delegate);
        if (incognitoTabCount > 0) {
            for (int i = 0; i < incognitoTabCount; i++) {
                incognitoTabModel.addTab(INCOGNITO_ID_OFFSET + tabCount + i);
            }
            TabModelUtils.setIndex(incognitoTabModel, 0);
        }
        initialize(false, tabModel, incognitoTabModel);
    }

    @Override
    public Tab openNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent,
            boolean incognito) {
        throw new UnsupportedOperationException();
    }

    @Override
    public void closeAllTabs() {
        throw new UnsupportedOperationException();
    }

    @Override
    public int getTotalTabCount() {
        throw new UnsupportedOperationException();
    }
}
