// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.android_webview.test.AwTestBase;
import org.chromium.content.browser.ContentVideoView;

import java.util.concurrent.Callable;

/**
 * Utils for testing SurfaceViews (SurfaceViews that display video).
 */
public class VideoSurfaceViewUtils {

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
