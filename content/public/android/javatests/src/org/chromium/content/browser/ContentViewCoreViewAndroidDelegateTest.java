// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import org.chromium.content_shell_apk.ContentShellTestBase;
import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * This suite verifies that when {@link ContentViewCore} replaces container
 * views then:
 * 1. anchor views are transferred between container views
 * 2. and the reference returned by {@link ContentViewCore#getViewAndroidDelegate()} is
 *    still valid and it will add/remove anchor views from the new container view.
 */
public class ContentViewCoreViewAndroidDelegateTest extends ContentShellTestBase {

    private ViewAndroidDelegate mViewAndroidDelegate;
    private ContentViewCore mContentViewCore;
    private ViewGroup mContainerView;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentViewCore = new ContentViewCore(getActivity());
        mContainerView = new FrameLayout(getActivity());

        mContentViewCore.initPopupZoomer(getActivity());
        // This reference never changes during the duration of the tests,
        // but can still be used to add/remove anchor views from the
        // updated container view.
        mViewAndroidDelegate = ViewAndroidDelegate.createBasicDelegate(mContainerView);
        mContentViewCore.setContainerView(mContainerView);
    }

    @SmallTest
    public void testAddAndRemoveAnchorViews() {
        assertEquals(0, mContainerView.getChildCount());

        // Add 2 anchor views
        View anchorView1 = addAnchorViewTest(mContainerView, 1);
        View anchorView2 = addAnchorViewTest(mContainerView, 2);

        // Remove anchorView1
        removeAnchorViewTest(mContainerView, anchorView1, 1);
        assertSame(anchorView2, mContainerView.getChildAt(0));

        // Try to remove anchorView1 again; no-op.
        removeAnchorViewTest(mContainerView, anchorView1, 1);
        assertSame(anchorView2, mContainerView.getChildAt(0));

        // Remove anchorView2
        removeAnchorViewTest(mContainerView, anchorView2, 0);
    }

    @SmallTest
    public void testAddAndMoveAnchorView() {
        // Add anchorView and set layout params
        View anchorView = addAnchorViewTest(mContainerView, 1);
        LayoutParams originalLayoutParams = setLayoutParams(anchorView, 0, 0);

        // Move it
        LayoutParams updatedLayoutParams = setLayoutParams(anchorView, 1, 2);
        assertEquals(1, mContainerView.getChildCount());
        assertFalse(areEqual(originalLayoutParams, updatedLayoutParams));

        // Move it back to the original position
        updatedLayoutParams = setLayoutParams(anchorView, 0, 0);
        assertEquals(1, mContainerView.getChildCount());
        assertTrue(areEqual(originalLayoutParams, updatedLayoutParams));
    }

    private View addAnchorViewTest(ViewGroup containerView, int expectedCount) {
        View anchorView = mViewAndroidDelegate.acquireView();
        assertEquals(expectedCount, containerView.getChildCount());
        assertSame(anchorView, containerView.getChildAt(expectedCount - 1));
        return anchorView;
    }

    private void removeAnchorViewTest(
            ViewGroup containerView, View anchorView1, int expectedCount) {
        mViewAndroidDelegate.removeView(anchorView1);
        assertEquals(expectedCount, containerView.getChildCount());
    }

    private LayoutParams setLayoutParams(View anchorView,
            int coordinatesValue, int dimensionsValue) {
        mViewAndroidDelegate.setViewPosition(anchorView, coordinatesValue, coordinatesValue,
                dimensionsValue, dimensionsValue, 1f, 10, 10);
        return anchorView.getLayoutParams();
    }

    private boolean areEqual(LayoutParams params1, LayoutParams params2) {
        return params1.height == params2.height && params1.width == params2.width;
    }
}
