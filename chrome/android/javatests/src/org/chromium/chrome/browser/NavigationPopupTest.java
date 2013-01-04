// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.graphics.Bitmap;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.NavigationPopup.NavigationPopupDelegate;
import org.chromium.chrome.testshell.ChromiumTestShellActivity;
import org.chromium.chrome.testshell.ChromiumTestShellTestBase;
import org.chromium.content.browser.NavigationClient;
import org.chromium.content.browser.NavigationEntry;
import org.chromium.content.browser.NavigationHistory;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for the navigation popup.
 */
public class NavigationPopupTest extends ChromiumTestShellTestBase {

    private static final int INVALID_NAVIGATION_INDEX = -1;

    private ChromiumTestShellActivity mActivity;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mActivity = launchChromiumTestShellWithBlankPage();
    }

    // Exists solely to expose protected methods to this test.
    private static class TestNavigationHistory extends NavigationHistory {
        @Override
        protected void addEntry(NavigationEntry entry) {
            super.addEntry(entry);
        }
    }

    // Exists solely to expose protected methods to this test.
    private static class TestNavigationEntry extends NavigationEntry {
        public TestNavigationEntry(int index, String url, String virtualUrl, String originalUrl,
                String title, Bitmap favicon) {
            super(index, url, virtualUrl, originalUrl, title, favicon);
        }
    }

    private static class TestNavigationPopupDelegate implements NavigationPopupDelegate {
        private boolean mHistoryRequested;

        @Override
        public void openHistory() {
            mHistoryRequested = true;
        }
    }

    private static class TestNavigationClient implements NavigationClient {
        private TestNavigationHistory mHistory;
        private int mNavigatedIndex = INVALID_NAVIGATION_INDEX;

        public TestNavigationClient() {
            mHistory = new TestNavigationHistory();
            mHistory.addEntry(new TestNavigationEntry(
                    1, "about:blank", null, null, "About Blank", null));
            mHistory.addEntry(new TestNavigationEntry(
                    5, "data:text/html;utf-8,<html>1</html>", null, null, null, null));
        }

        @Override
        public NavigationHistory getDirectedNavigationHistory(boolean isForward, int itemLimit) {
            return mHistory;
        }

        @Override
        public void goToNavigationIndex(int index) {
            mNavigatedIndex = index;
        }
    }

    @MediumTest
    @Feature({"Navigation"})
    public void testFaviconFetching() throws InterruptedException {
        final TestNavigationClient client = new TestNavigationClient();
        final NavigationPopup popup = new NavigationPopup(
                mActivity, new TestNavigationPopupDelegate(), client, true);
        popup.setWidth(300);
        popup.setAnchorView(mActivity.getActiveContentView());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                popup.show();
            }
        });

        assertTrue("All favicons did not get updated.",
                CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
                        @Override
                        public Boolean call() throws Exception {
                            NavigationHistory history = client.mHistory;
                            for (int i = 0; i < history.getEntryCount(); i++) {
                                if (history.getEntryAtIndex(i).getFavicon() == null) return false;
                            }
                            return true;
                        }
                    });
                } catch (ExecutionException e) {
                    return false;
                }
            }
        }));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                popup.dismiss();
            }
        });
    }

    @SmallTest
    @Feature({"Navigation"})
    public void testItemSelection() {
        final TestNavigationClient client = new TestNavigationClient();
        final NavigationPopup popup = new NavigationPopup(
                mActivity, new TestNavigationPopupDelegate(), client, true);
        popup.setWidth(300);
        popup.setAnchorView(mActivity.getActiveContentView());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                popup.show();
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                popup.performItemClick(1);
            }
        });

        assertFalse("Popup did not hide as expected.", popup.isShowing());
        assertEquals("Popup attempted to navigate to the wrong index", 5, client.mNavigatedIndex);
    }

    @SmallTest
    @Feature({"Navigation"})
    public void testShowHistorySelection() {
        final TestNavigationClient client = new TestNavigationClient();
        TestNavigationPopupDelegate delegate = new TestNavigationPopupDelegate();
        final NavigationPopup popup = new NavigationPopup(mActivity, delegate, client, true);
        popup.setWidth(300);
        popup.setAnchorView(mActivity.getActiveContentView());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                popup.show();
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                popup.performItemClick(2);
            }
        });

        assertFalse("Popup did not hide as expected.", popup.isShowing());
        assertTrue("Popup did not correctly request history.", delegate.mHistoryRequested);
        assertEquals("Popup attempted to navigate instead of showing history",
                INVALID_NAVIGATION_INDEX, client.mNavigatedIndex);
    }

}
