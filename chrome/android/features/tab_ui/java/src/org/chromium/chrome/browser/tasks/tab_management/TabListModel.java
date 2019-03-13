// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_ID;

import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A {@link PropertyListModel} implementation to keep information about a list of
 * {@link org.chromium.chrome.browser.tab.Tab}s.
 */
class TabListModel extends PropertyListModel<PropertyModel, PropertyKey> {
    /**
     * Convert the given tab ID to an index to match during partial updates.
     * @param tabId The tab ID to search for.
     * @return The index within the model {@link org.chromium.ui.modelutil.SimpleList}.
     */
    public int indexFromId(int tabId) {
        for (int i = 0; i < size(); i++) {
            if (get(i).get(TAB_ID) == tabId) return i;
        }
        return TabModel.INVALID_TAB_INDEX;
    }
}
