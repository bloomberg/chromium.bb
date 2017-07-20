// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.app.Instrumentation;
import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.browser.tabmodel.document.MockStorageDelegate;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

// TODO(yolandyan): move this class to its test rule once JUnit4 migration is over
final class MultiActivityTestCommon {
    private static final String TAG = "MultiActivityTest";

    private final MultiActivityTestCommonCallback mCallback;
    MockStorageDelegate mStorageDelegate;
    Context mContext;

    MultiActivityTestCommon(MultiActivityTestCommonCallback callback) {
        mCallback = callback;
    }

    void setUp() throws Exception {
        RecordHistogram.setDisabledForTests(true);
        mContext = mCallback.getInstrumentation().getTargetContext();
        ApplicationTestUtils.setUp(mContext, true);

        // Make the DocumentTabModelSelector use a mocked out directory so that test runs don't
        // interfere with each other.
        mStorageDelegate = new MockStorageDelegate(mContext.getCacheDir());
        DocumentTabModelSelector.setStorageDelegateForTests(mStorageDelegate);
    }

    void tearDown() throws Exception {
        mStorageDelegate.ensureDirectoryDestroyed();
        ApplicationTestUtils.tearDown(mContext);
        RecordHistogram.setDisabledForTests(false);
    }

    void waitForFullLoad(final ChromeActivity activity, final String expectedTitle) {
        waitForTabCreation(activity);
        ApplicationTestUtils.assertWaitForPageScaleFactorMatch(activity, 0.5f);
        final Tab tab = activity.getActivityTab();
        assert tab != null;

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (!tab.isLoadingAndRenderingDone()) return false;
                if (!TextUtils.equals(expectedTitle, tab.getTitle())) return false;
                return true;
            }
        });
    }

    private void waitForTabCreation(final ChromeActivity activity) {
        final CountDownLatch latch = new CountDownLatch(1);
        activity.getTabModelSelector().addObserver(new TabModelSelectorObserver() {
            public void onChange() {}
            public void onNewTabCreated(Tab tab) {
                latch.countDown();
            }
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {}
            public void onTabStateInitialized() {}

        });
        try {
            latch.await(CallbackHelper.WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Log.w(TAG, "CountDownLatch interrupted. The test may fail.");
        }
    }

    public interface MultiActivityTestCommonCallback { Instrumentation getInstrumentation(); }
}
