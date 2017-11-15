// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.app.Instrumentation;
import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.browser.tabmodel.document.MockStorageDelegate;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.TimeoutException;

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

    void waitForFullLoad(final ChromeActivity activity, final String expectedTitle)
            throws InterruptedException, TimeoutException {
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

    private void waitForTabCreation(ChromeActivity activity)
            throws InterruptedException, TimeoutException {
        final CallbackHelper newTabCreatorHelper = new CallbackHelper();
        activity.getTabModelSelector().addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onNewTabCreated(Tab tab) {
                newTabCreatorHelper.notifyCalled();
            }
        });
        newTabCreatorHelper.waitForCallback(0);
    }

    public interface MultiActivityTestCommonCallback { Instrumentation getInstrumentation(); }
}
