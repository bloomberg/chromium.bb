// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests related to the ToolbarProgressBar.
 */
public class ToolbarProgressBarTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    static final int TEST_WAIT_TIME_MS = 60000;

    public ToolbarProgressBarTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    /**
     * Test that calling progressBar.setProgress(# > 0) followed by progressBar.setProgress(0)
     * results in a hidden progress bar.
     * @throws InterruptedException
     */
    @Feature({"Android-Toolbar"})
    @MediumTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    public void testProgressBarDisappearsAfterFastShowHide() throws InterruptedException {
        // onAnimationEnd will be signaled on progress bar showing/hiding animation end.
        final Object onAnimationEnd = new Object();
        final AtomicBoolean animationEnded = new AtomicBoolean(false);
        final AtomicReference<ToolbarProgressBar> progressBar =
                new AtomicReference<ToolbarProgressBar>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                progressBar.set((ToolbarProgressBar) getActivity().findViewById(R.id.progress));
                progressBar.get().setAlphaAnimationDuration(10);
                progressBar.get().setHidingDelay(10);
                progressBar.get().animate().setListener(new AnimatorListener() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                    }

                    @Override
                    public void onAnimationRepeat(Animator animation) {
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        synchronized (onAnimationEnd) {
                            animationEnded.set(true);
                            onAnimationEnd.notify();
                        }
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {
                    }
                });
            }
        });

        // Before the actual test, ensure that the progress bar is hidden.
        assertNotSame(View.VISIBLE, progressBar.get().getVisibility());

        // Make some progress and check that the progress bar is fully visible.
        animationEnded.set(false);
        synchronized (onAnimationEnd) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    progressBar.get().start();
                    progressBar.get().setProgress(0.5f);
                }
            });

            long deadline = System.currentTimeMillis() + TEST_WAIT_TIME_MS;
            while (!animationEnded.get() && System.currentTimeMillis() < deadline) {
                onAnimationEnd.wait(deadline - System.currentTimeMillis());
            }
            assertEquals(1.0f, progressBar.get().getAlpha());
            assertEquals(View.VISIBLE, progressBar.get().getVisibility());
        }

        // Clear progress and check that the progress bar is hidden.
        animationEnded.set(false);
        synchronized (onAnimationEnd) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    progressBar.get().finish(true);
                }
            });

            long deadline = System.currentTimeMillis() + TEST_WAIT_TIME_MS;
            while (!animationEnded.get() && System.currentTimeMillis() < deadline) {
                onAnimationEnd.wait(deadline - System.currentTimeMillis());
            }
            assertEquals(0.0f, progressBar.get().getAlpha());
            assertNotSame(View.VISIBLE, progressBar.get().getVisibility());
        }
    }
}
