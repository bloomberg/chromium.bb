// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.app.Instrumentation;
import android.text.TextUtils;
import android.view.View;

import org.junit.Assert;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.content.browser.test.util.TestTouchUtils;

import java.util.concurrent.TimeoutException;

// TODO(yolandyan): Remove this class once all tests have been migrated to JUnit4.
final class ChromeTabbedActivityTestCommon {
    private final ChromeTabbedActivityTestCommonCallback mCallback;

    ChromeTabbedActivityTestCommon(ChromeTabbedActivityTestCommonCallback callback) {
        mCallback = callback;
    }

    void loadUrlInManyNewTabs(final String url, final int numTabs) throws InterruptedException {
        final CallbackHelper[] pageLoadedCallbacks = new CallbackHelper[numTabs];
        final int[] tabIds = new int[numTabs];
        for (int i = 0; i < numTabs; ++i) {
            final int index = i;
            mCallback.getInstrumentation().runOnMainSync(new Runnable() {
                @Override
                public void run() {
                    Tab currentTab = mCallback.getActivity().getCurrentTabCreator().launchUrl(
                            url, TabLaunchType.FROM_LINK);
                    final CallbackHelper pageLoadCallback = new CallbackHelper();
                    pageLoadedCallbacks[index] = pageLoadCallback;
                    currentTab.addObserver(new EmptyTabObserver() {
                        @Override
                        public void onPageLoadFinished(Tab tab) {
                            pageLoadCallback.notifyCalled();
                            tab.removeObserver(this);
                        }
                    });
                    tabIds[index] = currentTab.getId();
                }
            });
        }
        //  When opening many tabs some may be frozen due to memory pressure and won't send
        //  PAGE_LOAD_FINISHED events. Iterate over the newly opened tabs and wait for each to load.
        for (int i = 0; i < numTabs; ++i) {
            final TabModel tabModel = mCallback.getActivity().getCurrentTabModel();
            final Tab tab = TabModelUtils.getTabById(tabModel, tabIds[i]);
            mCallback.getInstrumentation().runOnMainSync(new Runnable() {
                @Override
                public void run() {
                    TabModelUtils.setIndex(tabModel, tabModel.indexOf(tab));
                }
            });
            try {
                pageLoadedCallbacks[i].waitForCallback(0);
            } catch (TimeoutException e) {
                Assert.fail("PAGE_LOAD_FINISHED was not received for tabId=" + tabIds[i]);
            }
        }
    }

    void invokeContextMenuAndOpenInANewTab(View view, int contextMenuItemId,
            boolean expectIncognito, final String expectedUrl) throws InterruptedException {
        final CallbackHelper createdCallback = new CallbackHelper();
        final TabModel tabModel =
                mCallback.getActivity().getTabModelSelector().getModel(expectIncognito);
        tabModel.addObserver(new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, TabLaunchType type) {
                if (TextUtils.equals(expectedUrl, tab.getUrl())) {
                    createdCallback.notifyCalled();
                    tabModel.removeObserver(this);
                }
            }
        });

        TestTouchUtils.longClickView(mCallback.getInstrumentation(), view);
        Assert.assertTrue(mCallback.getInstrumentation().invokeContextMenuAction(
                mCallback.getActivity(), contextMenuItemId, 0));

        try {
            createdCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Never received tab creation event");
        }

        if (expectIncognito) {
            Assert.assertTrue(mCallback.getActivity().getTabModelSelector().isIncognitoSelected());
        } else {
            Assert.assertFalse(mCallback.getActivity().getTabModelSelector().isIncognitoSelected());
        }
    }

    public interface ChromeTabbedActivityTestCommonCallback {
        ChromeTabbedActivity getActivity();
        Instrumentation getInstrumentation();
    }
}
