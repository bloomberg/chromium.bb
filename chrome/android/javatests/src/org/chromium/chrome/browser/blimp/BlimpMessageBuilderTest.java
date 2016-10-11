// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.blimp;

import android.app.Activity;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.blimp.ui.BlimpMessageBuilder;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Test BlimpMessageBuilder.
 */
public class BlimpMessageBuilderTest extends ChromeTabbedActivityTestBase {
    private static final String TEST_MSG = "Snackbar show, snackbar.";
    private SnackbarManager mManager;

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
        mManager = getActivity().getSnackbarManager();
    }

    private static class MockBlimpMessageBuilder extends BlimpMessageBuilder {
        Snackbar getSnackbar() {
            return super.getSnackbarForTest();
        }

        // Override this method to expose visibility for this test.
        @Override
        public SnackbarManager getSnackbarManager(Activity activity) {
            return super.getSnackbarManager(activity);
        }
    }

    /**
     * Test if we can show {@link Snackbar} in ChromeTabbedActivity.
     */
    @MediumTest
    public void testSnackbarMessage() throws Exception {
        final MockBlimpMessageBuilder mockBuilder = new MockBlimpMessageBuilder();
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        assertTrue(activity instanceof SnackbarManager.SnackbarManageable);
        assertNotNull(mockBuilder.getSnackbarManager(activity));

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mockBuilder.showMessage(TEST_MSG);
                assertNotNull("Builder should hold a snackbar.", mockBuilder.getSnackbar());
            }
        });

        // Wait for the {@link Snackbar} to show within default timeout of {@link CriteriaHelper}.
        CriteriaHelper.pollUiThread(new Criteria("Snackbar should show.") {
            @Override
            public boolean isSatisfied() {
                return mManager.isShowing()
                        && (mManager.getCurrentSnackbarForTesting() == mockBuilder.getSnackbar());
            }
        });
    }
}
