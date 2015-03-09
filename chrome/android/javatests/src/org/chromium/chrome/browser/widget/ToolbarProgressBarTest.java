// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_PHONE;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.shell.R;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests related to the ToolbarProgressBar.
 */
public class ToolbarProgressBarTest extends ChromeShellTestBase {
    /**
     * Test that calling progressBar.setProgress(# > 0) followed by progressBar.setProgress(0)
     * results in a hidden progress bar (the secondary progress needs to be 0).
     * @throws InterruptedException
     */
    @Feature({"Android-Toolbar"})
    @MediumTest
    @Restriction(RESTRICTION_TYPE_PHONE)
    public void testProgressBarDisappearsAfterFastShowHide() throws InterruptedException {
        launchChromeShellWithUrl("about:blank");
        waitForActiveShellToBeDoneLoading();

        final AtomicReference<ToolbarProgressBar> progressBar =
                new AtomicReference<ToolbarProgressBar>();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                progressBar.set((ToolbarProgressBar) getActivity().findViewById(R.id.progress));
            }
        });

        // Wait for the progress bar to be reset.
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return progressBar.get().getProgress() == 0;
            }
        });

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                assertEquals("Progress bar should be hidden to start.", 0,
                        progressBar.get().getProgress());
                progressBar.get().setProgress(10);
                assertTrue("Progress bar did not start animating",
                        progressBar.get().isAnimatingForShowOrHide());
                progressBar.get().setProgress(0);
            }
        });

        // Wait for the progress bar to finish any and all animations.
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !progressBar.get().isAnimatingForShowOrHide();
            }
        });

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // The secondary progress should be gone.
                assertEquals("Progress bar background still visible.", 0,
                        progressBar.get().getSecondaryProgress());
            }
        });
    }
}