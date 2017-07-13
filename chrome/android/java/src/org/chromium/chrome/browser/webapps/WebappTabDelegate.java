// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.net.Uri;
import android.support.customtabs.CustomTabsIntent;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;

/**
 * Asynchronously creates Tabs for navigation originating from an installed PWA.
 *
 * This is the same as the parent class with exception of opening a Custom Tab for
 * {@code _blank} links and {@code window.open(url)} calls instead of creating a new tab in Chrome.
 */
public class WebappTabDelegate extends TabDelegate {
    public WebappTabDelegate(boolean incognito) {
        super(incognito);
    }

    @Override
    public void createNewTab(AsyncTabCreationParams asyncParams, TabLaunchType type, int parentId) {
        int assignedTabId = TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID);
        AsyncTabParamsManager.add(assignedTabId, asyncParams);

        Intent intent = new CustomTabsIntent.Builder().setShowTitle(true).build().intent;
        intent.setData(Uri.parse(asyncParams.getLoadUrlParams().getUrl()));
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_SEND_TO_EXTERNAL_DEFAULT_HANDLER, true);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_IS_OPENED_BY_CHROME, true);
        addAsyncTabExtras(asyncParams, parentId, false /* isChromeUI */, assignedTabId, intent);

        IntentHandler.startActivityForTrustedIntent(intent);
    }
}
