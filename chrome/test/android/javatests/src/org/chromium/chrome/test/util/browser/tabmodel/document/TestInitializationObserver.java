// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.tabmodel.document;

import static junit.framework.Assert.assertTrue;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Monitors the DocumentTabModel until it has progressed far enough along initialization.
 */
public class TestInitializationObserver extends DocumentTabModel.InitializationObserver {
    public boolean mIsReady;
    private final int mState;

    public TestInitializationObserver(DocumentTabModel model, int state) {
        super(model);
        mState = state;
    }

    @Override
    protected void runImmediately() {
        mIsReady = true;
    }

    @Override
    public boolean isSatisfied(int currentState) {
        return currentState >= mState;
    }

    @Override
    public boolean isCanceled() {
        return false;
    }

    /**
     * Wait until the DocumentTabModel has reached the given state.
     * @param model DocumentTabModel to monitor.
     * @param state State to wait for.
     */
    public static void waitUntilState(final DocumentTabModel model, int state) throws Exception {
        final TestInitializationObserver observer = new TestInitializationObserver(model, state);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                observer.runWhenReady();
            }
        });

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return observer.mIsReady;
            }
        }));
    }
}