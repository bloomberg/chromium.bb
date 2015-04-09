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

        mContentViewCore.createContentViewAndroidDelegate();
        mContentViewCore.initPopupZoomer(getActivity());
        // This reference never changes during the duration of the tests,
        // but can still be used to add/remove anchor views from the
        // updated container view.
        mViewAndroidDelegate = mContentViewCore.getViewAndroidDelegate();
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

    @SmallTest
    public void testMovedAndRemovedAnchorViewIsNotTransferred() {
        // Add, move and remove anchorView
        View anchorView = addAnchorViewTest(mContainerView, 1);
        setLayoutParams(anchorView, 1, 2);
        removeAnchorViewTest(mContainerView, anchorView, 0);

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView();

        // Verify that no anchor view is transferred between containerViews
        assertEquals(0, mContainerView.getChildCount());
        assertEquals(0, updatedContainerView.getChildCount());
    }

    @SmallTest
    public void testTransferAnchorView() {
        // Add anchor view
        View anchorView = addAnchorViewTest(mContainerView, 1);
        LayoutParams layoutParams = anchorView.getLayoutParams();

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView();

        verifyAnchorViewCorrectlyTransferred(anchorView, updatedContainerView, layoutParams);
    }

    @SmallTest
    public void testTransferMovedAnchorView() {
        // Add anchor view and move it
        View anchorView = addAnchorViewTest(mContainerView, 1);
        LayoutParams layoutParams = setLayoutParams(anchorView, 1, 2);

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView();

        verifyAnchorViewCorrectlyTransferred(anchorView, updatedContainerView, layoutParams);
    }

    @SmallTest
    public void testRemoveTransferedAnchorView() {
        // Add anchor view
        View anchorView = addAnchorViewTest(mContainerView, 1);

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView();

        verifyAnchorViewCorrectlyTransferred(anchorView, updatedContainerView);

        // Remove transferred anchor view
        removeAnchorViewTest(updatedContainerView, anchorView, 0);
    }

    @SmallTest
    public void testMoveTransferedAnchorView() {
        // Add anchor view
        View anchorView = addAnchorViewTest(mContainerView, 1);
        LayoutParams layoutParams = anchorView.getLayoutParams();

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView();

        verifyAnchorViewCorrectlyTransferred(anchorView, updatedContainerView, layoutParams);

        // Move transferred anchor view
        assertFalse(areEqual(layoutParams, setLayoutParams(anchorView, 1, 2)));
    }

    @SmallTest
    public void testTransferMultipleMovedAnchorViews() {
        // Add and move anchorView1
        View anchorView1 = addAnchorViewTest(mContainerView, 1);
        LayoutParams layoutParams1 = setLayoutParams(anchorView1, 1, 2);

        // Add and move anchorView2
        View anchorView2 = addAnchorViewTest(mContainerView, 2);
        LayoutParams layoutParams2 = setLayoutParams(anchorView2, 2, 4);

        // Replace containerView
        FrameLayout updatedContainerView = updateContainerView();

        // Verify that anchor views are transfered in the same order
        // and with the same layout params.
        assertEquals(0, mContainerView.getChildCount());
        assertEquals(2, updatedContainerView.getChildCount());
        assertSame(anchorView1, updatedContainerView.getChildAt(0));
        assertTrue(areEqual(layoutParams1, anchorView1.getLayoutParams()));
        assertSame(anchorView2, updatedContainerView.getChildAt(1));
        assertTrue(areEqual(layoutParams2, anchorView2.getLayoutParams()));
    }

    private View addAnchorViewTest(ViewGroup containerView, int expectedCount) {
        View anchorView = mViewAndroidDelegate.acquireAnchorView();
        assertEquals(expectedCount, containerView.getChildCount());
        assertSame(anchorView, containerView.getChildAt(expectedCount - 1));
        return anchorView;
    }

    private void removeAnchorViewTest(
            ViewGroup containerView, View anchorView1, int expectedCount) {
        mViewAndroidDelegate.releaseAnchorView(anchorView1);
        assertEquals(expectedCount, containerView.getChildCount());
    }

    private LayoutParams setLayoutParams(View anchorView,
            int coordinatesValue, int dimensionsValue) {
        mViewAndroidDelegate.setAnchorViewPosition(anchorView, coordinatesValue, coordinatesValue,
                dimensionsValue, dimensionsValue);
        return anchorView.getLayoutParams();
    }

    private FrameLayout updateContainerView() {
        FrameLayout updatedContainerView = new FrameLayout(getActivity());
        mContentViewCore.setContainerView(updatedContainerView);
        return updatedContainerView;
    }

    private void verifyAnchorViewCorrectlyTransferred(View anchorView,
            FrameLayout updatedContainerView, LayoutParams expectedParams) {
        assertTrue(areEqual(expectedParams, anchorView.getLayoutParams()));
        verifyAnchorViewCorrectlyTransferred(anchorView, updatedContainerView);
    }

    private void verifyAnchorViewCorrectlyTransferred(View anchorView,
            FrameLayout updatedContainerView) {
        assertEquals(0, mContainerView.getChildCount());
        assertEquals(1, updatedContainerView.getChildCount());
        assertSame(anchorView, updatedContainerView.getChildAt(0));
    }

    private boolean areEqual(LayoutParams params1, LayoutParams params2) {
        return params1.height == params2.height && params1.width == params2.width;
    }
}
