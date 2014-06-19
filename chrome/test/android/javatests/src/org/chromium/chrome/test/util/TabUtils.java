// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.view.ContextMenu;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestContentViewClient;
import org.chromium.content.browser.test.util.TestContentViewClientWrapper;
import org.chromium.content.browser.test.util.TestWebContentsObserver;

import java.lang.ref.WeakReference;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A utility class that contains methods generic to all Tabs tests.
 */
public class TabUtils {
    private static TestContentViewClient createTestContentViewClientForTab(Tab tab) {
        ContentViewClient client = tab.getContentViewCore().getContentViewClient();
        if (client instanceof TestContentViewClient) return (TestContentViewClient) client;

        TestContentViewClient testClient = new TestContentViewClientWrapper(client);
        tab.getContentViewCore().setContentViewClient(testClient);
        return testClient;
    }

    /**
     * Provides some callback helpers when waiting for certain tab-based events to occur.
     */
    public static class TestCallbackHelperContainerForTab extends TestCallbackHelperContainer {
        private final CallbackHelper mOnCloseTabHelper;
        private final OnContextMenuShownHelper mOnContextMenuShownHelper;

        private TestCallbackHelperContainerForTab(Tab tab) {
            super(createTestContentViewClientForTab(tab),
                    new TestWebContentsObserver(tab.getContentViewCore()));
            mOnCloseTabHelper = new CallbackHelper();
            mOnContextMenuShownHelper = new OnContextMenuShownHelper();
            tab.addObserver(new EmptyTabObserver() {
                @Override
                public void onDestroyed(Tab tab) {
                    mOnCloseTabHelper.notifyCalled();
                }

                @Override
                public void onContextMenuShown(Tab tab, ContextMenu menu) {
                    mOnContextMenuShownHelper.notifyCalled(menu);
                }
            });
        }

        /**
         * Callback helper that also provides access to the last display ContextMenu.
         */
        public static class OnContextMenuShownHelper extends CallbackHelper {
            private WeakReference<ContextMenu> mContextMenu;

            public void notifyCalled(ContextMenu menu) {
                mContextMenu = new WeakReference<ContextMenu>(menu);
                notifyCalled();
            }

            public ContextMenu getContextMenu() {
                assert getCallCount() > 0;
                return mContextMenu.get();
            }
        }

        public CallbackHelper getOnCloseTabHelper() {
            return mOnCloseTabHelper;
        }

        public OnContextMenuShownHelper getOnContextMenuShownHelper() {
            return mOnContextMenuShownHelper;
        }
    }

    /**
     * Creates, binds and returns a TestCallbackHelperContainer for a given Tab.
     */
    public static TestCallbackHelperContainerForTab getTestCallbackHelperContainer(final Tab tab) {
        if (tab == null) {
            return null;
        }
        final AtomicReference<TestCallbackHelperContainerForTab> result =
                new AtomicReference<TestCallbackHelperContainerForTab>();
        // TODO(yfriedman): Change callers to be executed on the UI thread. Unfortunately this is
        // super convenient as the caller is nearly always on the test thread which is fine to block
        // and it's cumbersome to keep bouncing to the UI thread.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                result.set(new TestCallbackHelperContainerForTab(tab));
            }
        });
        return result.get();
    }
}
