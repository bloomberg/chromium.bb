// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.shell.R;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests related to the ToolbarProgressBar.
 */
public class ClipDrawableProgressBarTest extends ChromeShellTestBase {
    /**
     * Test that calling progressBar.setProgress(# > 0) followed by progressBar.setProgress(0)
     * results in a hidden progress bar (the secondary progress needs to be 0).
     * @throws InterruptedException
     */
    @Feature({"Android-Toolbar"})
    @MediumTest
    public void testProgressBarDisappearsAfterFastShowHide() throws InterruptedException {
        launchChromeShellWithUrl("about:blank");
        waitForActiveShellToBeDoneLoading();

        final AtomicReference<ClipDrawableProgressBar> progressBar =
                new AtomicReference<ClipDrawableProgressBar>();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                progressBar.set(
                        (ClipDrawableProgressBar) getActivity().findViewById(R.id.progress));
            }
        });

        // Make sure that there is some progress.
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return progressBar.get().getProgress() > 0;
            }
        });

        // Wait for the progress bar to be reset.
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return progressBar.get().getProgress() == 0;
            }
        });
    }
}