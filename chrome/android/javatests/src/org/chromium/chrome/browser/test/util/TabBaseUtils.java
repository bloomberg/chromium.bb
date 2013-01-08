// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test.util;

import org.chromium.chrome.browser.TabBase;
import org.chromium.chrome.browser.TabObserver;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestContentViewClient;
import org.chromium.content.browser.test.util.TestContentViewClientWrapper;
import org.chromium.content.browser.test.util.TestWebContentsObserver;
import org.chromium.content.browser.ContentViewClient;

/**
 * A utility class that contains methods generic to all Tabs tests.
 */
public class TabBaseUtils {
    private final static String TAG = TabBaseUtils.class.getSimpleName();

    private static TestContentViewClient createTestContentViewClientForTab(TabBase tab) {
        ContentViewClient client = tab.getContentView().getContentViewClient();
        if (client instanceof TestContentViewClient) return (TestContentViewClient) client;

        TestContentViewClient testClient = new TestContentViewClientWrapper(client);
        tab.getContentView().setContentViewClient(testClient);
        return testClient;
    }

    public static class TestCallbackHelperContainerForTab
            extends TestCallbackHelperContainer implements TabObserver {
        private OnCloseTabHelper mOnCloseTabHelper;
        public TestCallbackHelperContainerForTab(TabBase tab) {
            super(createTestContentViewClientForTab(tab),
                    new TestWebContentsObserver(tab.getContentView().getContentViewCore()));
            mOnCloseTabHelper = new OnCloseTabHelper();
            tab.addObserver(this);
        }

        public static class OnCloseTabHelper extends CallbackHelper {
        }

        public OnCloseTabHelper getOnCloseTabHelper() {
            return mOnCloseTabHelper;
        }

        @Override
        public void onLoadProgressChanged(TabBase tab, int progress) {
        }

        @Override
        public void onUpdateUrl(TabBase tab, String url) {
        }

        @Override
        public void onCloseTab(TabBase tab) {
            mOnCloseTabHelper.notifyCalled();
        }
    }

    /**
     * Creates, binds and returns a TestCallbackHelperContainer for a given Tab.
     */
    public static TestCallbackHelperContainerForTab getTestCallbackHelperContainer(
            final TabBase tab) {
        return tab == null ? null : new TestCallbackHelperContainerForTab(tab);
    }
}
