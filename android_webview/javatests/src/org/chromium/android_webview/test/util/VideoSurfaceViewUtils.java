// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.android_webview.test.AwTestBase;
import org.chromium.components.external_video_surface.ExternalVideoSurfaceContainer.NoPunchingSurfaceView;
import org.chromium.content.browser.ContentVideoView;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * Utils for testing SurfaceViews (SurfaceViews that display video).
 */
public class VideoSurfaceViewUtils {
    /**
     * MAX_WAIT_FOR_HOLE_PUNCHING_SURFACE_ATTACHED is the maximum time we expect
     * the Android system needs to setup a video hole surface.
     */
    private static final long MAX_WAIT_FOR_HOLE_PUNCHING_SURFACE_ATTACHED = scaleTimeout(1000);

    /**
     * Asserts that the given ViewGroup contains exactly one ContentVideoView.
     * @param test Test doing the assert.
     * @param view View or ViewGroup to traverse.
     */
    public static void assertContainsOneContentVideoView(AwTestBase test, View view)
                throws Exception {
        AwTestBase.assertEquals(
                1, containsNumChildrenOfType(test, view, ContentVideoView.class));
    }

    /**
     * Asserts that the given ViewGroup does not contain a video hole surface.
     * @param test Test doing the assert.
     * @param view View or ViewGroup to traverse.
     */
    public static void assertContainsZeroVideoHoleSurfaceViews(AwTestBase test, View view)
                throws Exception {
        AwTestBase.assertEquals(
                0, containsNumChildrenOfType(test, view, NoPunchingSurfaceView.class));
    }

    /**
     * Asserts that the given ViewGroup contains exactly one video hole surface.
     * @param test Test doing the assert.
     * @param view View or ViewGroup to traverse.
     */
    public static void assertContainsOneVideoHoleSurfaceView(AwTestBase test, View view)
            throws Exception {
        AwTestBase.assertEquals(
                1, containsNumChildrenOfType(test, view, NoPunchingSurfaceView.class));
    }

    /**
     * Waits the time needed for setting up a video hole surface and asserts that the given
     * ViewGroup contains exactly one such surface.
     * @param test Test doing the assert.
     * @param view View or ViewGroup to traverse.
     */
    public static void waitAndAssertContainsOneVideoHoleSurfaceView(AwTestBase test, View view)
                throws Exception {
        Thread.sleep(MAX_WAIT_FOR_HOLE_PUNCHING_SURFACE_ATTACHED);
        assertContainsOneVideoHoleSurfaceView(test, view);
    }

    /**
     * Waits the time that would have been needed for setting up a video hole surface and
     * asserts that the given ViewGroup does not contain such a surface.
     * @param test Test doing the assert.
     * @param view View or ViewGroup to traverse.
     */
    public static void waitAndAssertContainsZeroVideoHoleSurfaceViews(AwTestBase test, View view)
                throws Exception {
        Thread.sleep(MAX_WAIT_FOR_HOLE_PUNCHING_SURFACE_ATTACHED);
        assertContainsZeroVideoHoleSurfaceViews(test, view);
    }

    /**
     * Polls until the given ViewGroup contains one video hole surface.
     * @param test Test doing the assert.
     * @param view View or ViewGroup to traverse.
     */
    public static void pollAndAssertContainsOneVideoHoleSurfaceView(final AwTestBase test,
            final View view) throws InterruptedException {
        AwTestBase.assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return containsNumChildrenOfType(test, view, NoPunchingSurfaceView.class) == 1;
                } catch (Exception e) {
                    AwTestBase.fail(e.getMessage());
                    return false;
                }
            }
        }, MAX_WAIT_FOR_HOLE_PUNCHING_SURFACE_ATTACHED,
        MAX_WAIT_FOR_HOLE_PUNCHING_SURFACE_ATTACHED / 10));
    }

    private static int containsNumChildrenOfType(final AwTestBase test,
            final View view,
            final Class<? extends View> childType) throws Exception {
        return test.runTestOnUiThreadAndGetResult(new Callable<Integer>() {
            @Override
            public Integer call() throws Exception {
                return containsNumChildrenOfTypeOnUiThread(view, childType);
            }
        });
    }

    private static int containsNumChildrenOfTypeOnUiThread(final View view,
            final Class<? extends View> childType) throws Exception {
        return containsNumChildrenOfTypeOnUiThread(view, childType, 0);
    }

    private static int containsNumChildrenOfTypeOnUiThread(final View view,
            final Class<? extends View> childType, int sum) throws Exception {
        if (childType.isInstance(view)) return 1;

        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                sum += containsNumChildrenOfTypeOnUiThread(viewGroup.getChildAt(i), childType);
            }
        }
        return sum;
    }
}
