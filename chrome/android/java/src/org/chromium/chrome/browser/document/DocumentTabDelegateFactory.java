// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tabmodel.TabModel;

/**
 * A {@link TabDelegateFactory} class to be used in all {@link Tab} owned
 * by a {@link DocumentActivity}.
 */
public class DocumentTabDelegateFactory extends TabDelegateFactory {
    /**
     * A web contents delegate for handling opening new windows in Document mode.
     */
    private static class DocumentTabWebContentsDelegateAndroid
            extends TabWebContentsDelegateAndroid {
        private boolean mIsIncognito;

        public DocumentTabWebContentsDelegateAndroid(Tab tab) {
            super(tab);
            mIsIncognito = tab.isIncognito();
        }

        /**
         * TODO(dfalcantara): Remove this when DocumentActivity.getTabModelSelector()
         *                    can return a TabModelSelector that activateContents() can use.
         */
        @Override
        protected TabModel getTabModel() {
            return ChromeApplication.getDocumentTabModelSelector().getModel(mIsIncognito);
        }
    }

    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        return new DocumentTabWebContentsDelegateAndroid(tab);
    }
}
