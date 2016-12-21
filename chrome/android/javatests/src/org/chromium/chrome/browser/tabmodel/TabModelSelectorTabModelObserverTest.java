// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.support.test.filters.SmallTest;
import android.test.UiThreadTest;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the TabModelSelectorTabModelObserver.
 */
public class TabModelSelectorTabModelObserverTest extends TabModelSelectorObserverTestBase {
    @UiThreadTest
    @SmallTest
    public void testAlreadyInitializedSelector() throws InterruptedException, TimeoutException {
        final CallbackHelper registrationCompleteCallback = new CallbackHelper();
        TabModelSelectorTabModelObserver observer =
                new TabModelSelectorTabModelObserver(mSelector) {
                    @Override
                    protected void onRegistrationComplete() {
                        registrationCompleteCallback.notifyCalled();
                    }
                };
        registrationCompleteCallback.waitForCallback(0);
        assertAllModelsHaveObserver(mSelector, observer);
    }

    @UiThreadTest
    @SmallTest
    public void testUninitializedSelector() throws InterruptedException, TimeoutException {
        mSelector = new TabModelSelectorBase() {
            @Override
            public Tab openNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent,
                    boolean incognito) {
                return null;
            }
        };
        final CallbackHelper registrationCompleteCallback = new CallbackHelper();
        TabModelSelectorTabModelObserver observer =
                new TabModelSelectorTabModelObserver(mSelector) {
                    @Override
                    protected void onRegistrationComplete() {
                        registrationCompleteCallback.notifyCalled();
                    }
                };
        mSelector.initialize(false, mNormalTabModel, mIncognitoTabModel);
        registrationCompleteCallback.waitForCallback(0);
        assertAllModelsHaveObserver(mSelector, observer);
    }

    private static void assertAllModelsHaveObserver(
            TabModelSelector selector, TabModelObserver observer) {
        List<TabModel> models = selector.getModels();
        for (int i = 0; i < models.size(); i++) {
            assertTrue(models.get(i) instanceof TabModelSelectorTestTabModel);
            assertTrue(((TabModelSelectorTestTabModel) models.get(i))
                    .getObservers().contains(observer));
        }
    }
}
