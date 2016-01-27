// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar;

import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Tests for {@link SnackbarManager}.
 */
public class SnackbarTest extends ChromeTabbedActivityTestBase {
    SnackbarManager mManager;
    SnackbarController mDefaultController = new SnackbarController() {
        @Override
        public void onDismissNoAction(Object actionData) {
        }

        @Override
        public void onAction(Object actionData) {
        }
    };

    @Override
    public void startMainActivity() throws InterruptedException {
        SnackbarManager.setDurationForTesting(1000);
        startMainActivityOnBlankPage();
        mManager = getActivity().getSnackbarManager();
    }

    @MediumTest
    public void testStackQueueOrder() throws InterruptedException {
        final Snackbar stackbar = Snackbar.make("stack", mDefaultController,
                Snackbar.TYPE_ACTION);
        final Snackbar queuebar = Snackbar.make("queue", mDefaultController,
                Snackbar.TYPE_NOTIFICATION);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mManager.showSnackbar(stackbar);
            }
        });
        CriteriaHelper.pollForUIThreadCriteria(new Criteria("First snackbar not shown") {
            @Override
            public boolean isSatisfied() {
                return mManager.isShowing() && mManager.getCurrentSnackbarForTesting() == stackbar;
            }
        });
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mManager.showSnackbar(queuebar);
                assertTrue("Snackbar not showing", mManager.isShowing());
                assertEquals("Snackbars on stack should not be cancled by snackbars on queue",
                        stackbar, mManager.getCurrentSnackbarForTesting());
            }
        });
        CriteriaHelper.pollForUIThreadCriteria(new Criteria("Snackbar on queue not shown") {
            @Override
            public boolean isSatisfied() {
                return mManager.isShowing() && mManager.getCurrentSnackbarForTesting() == queuebar;
            }
        });
        CriteriaHelper.pollForUIThreadCriteria(new Criteria("Snackbar did not time out") {
            @Override
            public boolean isSatisfied() {
                return !mManager.isShowing();
            }
        });
    }

    @SmallTest
    public void testQueueStackOrder() throws InterruptedException {
        final Snackbar stackbar = Snackbar.make("stack", mDefaultController,
                Snackbar.TYPE_ACTION);
        final Snackbar queuebar = Snackbar.make("queue", mDefaultController,
                Snackbar.TYPE_NOTIFICATION);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mManager.showSnackbar(queuebar);
            }
        });
        CriteriaHelper.pollForUIThreadCriteria(new Criteria("First snackbar not shown") {
            @Override
            public boolean isSatisfied() {
                return mManager.isShowing() && mManager.getCurrentSnackbarForTesting() == queuebar;
            }
        });
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mManager.showSnackbar(stackbar);
            }
        });
        CriteriaHelper.pollForUIThreadCriteria(
                new Criteria("Snackbar on queue was not cleared by snackbar stack.") {
                    @Override
                    public boolean isSatisfied() {
                        return mManager.isShowing()
                                && mManager.getCurrentSnackbarForTesting() == stackbar;
                    }
                });
        CriteriaHelper.pollForUIThreadCriteria(new Criteria("Snackbar did not time out") {
            @Override
            public boolean isSatisfied() {
                return !mManager.isShowing();
            }
        });
    }
}
