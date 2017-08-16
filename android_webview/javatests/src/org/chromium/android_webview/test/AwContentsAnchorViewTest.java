// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;
import android.test.UiThreadTest;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import org.chromium.android_webview.AwViewAndroidDelegate;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.RenderCoordinates;
import org.chromium.ui.display.DisplayAndroid;

/**
 * Tests anchor views are correctly added/removed when their container view is updated.
 */
public class AwContentsAnchorViewTest extends AwTestBase {

    private FrameLayout mContainerView;
    private AwViewAndroidDelegate mViewDelegate;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContainerView = new FrameLayout(getActivity());
        mViewDelegate = new AwViewAndroidDelegate(mContainerView, null, new RenderCoordinates());
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @UiThreadTest
    public void testAddAndRemoveAnchorViews() {
        // Add 2 anchor views
        View anchorView1 = addAnchorView();
        View anchorView2 = addAnchorView();

        // Remove anchorView1
        removeAnchorView(anchorView1);

        // Try to remove anchorView1 again; no-op.
        removeAnchorView(anchorView1);

        // Remove anchorView2
        removeAnchorView(anchorView2);
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @UiThreadTest
    public void testAddAndMoveAnchorView() {
        // Add anchorView and set layout params
        View anchorView = addAnchorView();
        LayoutParams originalLayoutParams = setLayoutParams(anchorView, 0, 0);

        // Move it
        LayoutParams updatedLayoutParams = setLayoutParams(anchorView, 1, 2);
        assertFalse(areEqual(originalLayoutParams, updatedLayoutParams));

        // Move it back to the original position
        updatedLayoutParams = setLayoutParams(anchorView, 0, 0);
        assertTrue(areEqual(originalLayoutParams, updatedLayoutParams));
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @UiThreadTest
    public void testMovedAndRemovedAnchorViewIsNotTransferred() throws Throwable {
        // Add, move and remove anchorView
        View anchorView = addAnchorView();
        setLayoutParams(anchorView, 1, 2);
        removeAnchorView(anchorView);

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView();

        // Verify that no anchor view is transferred between containerViews
        assertFalse(isViewInContainer(mContainerView, anchorView));
        assertFalse(isViewInContainer(updatedContainerView, anchorView));
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @UiThreadTest
    public void testTransferAnchorView() throws Throwable {
        // Add anchor view
        View anchorView = addAnchorView();
        LayoutParams layoutParams = anchorView.getLayoutParams();

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView();
        verifyAnchorViewCorrectlyTransferred(
                mContainerView, anchorView, updatedContainerView, layoutParams);
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @UiThreadTest
    public void testTransferMovedAnchorView() throws Throwable {
        // Add anchor view and move it
        View anchorView = addAnchorView();
        LayoutParams layoutParams = setLayoutParams(anchorView, 1, 2);

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView();

        verifyAnchorViewCorrectlyTransferred(
                mContainerView, anchorView, updatedContainerView, layoutParams);
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @UiThreadTest
    public void testRemoveTransferedAnchorView() throws Throwable {
        // Add anchor view
        View anchorView = addAnchorView();

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView();

        verifyAnchorViewCorrectlyTransferred(mContainerView, anchorView, updatedContainerView);

        // Remove transferred anchor view
        removeAnchorView(anchorView);
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @UiThreadTest
    public void testMoveTransferedAnchorView() throws Throwable {
        // Add anchor view
        View anchorView = addAnchorView();
        LayoutParams layoutParams = anchorView.getLayoutParams();

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView();

        verifyAnchorViewCorrectlyTransferred(
                mContainerView, anchorView, updatedContainerView, layoutParams);

        // Move transferred anchor view
        assertFalse(areEqual(layoutParams, setLayoutParams(anchorView, 1, 2)));
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @UiThreadTest
    public void testTransferMultipleMovedAnchorViews() throws Throwable {
        // Add and move anchorView1
        View anchorView1 = addAnchorView();
        LayoutParams layoutParams1 = setLayoutParams(anchorView1, 1, 2);

        // Add and move anchorView2
        View anchorView2 = addAnchorView();
        LayoutParams layoutParams2 = setLayoutParams(anchorView2, 2, 4);

        // Replace containerView
        FrameLayout updatedContainerView = updateContainerView();

        // Verify that anchor views are transfered with the same layout params.
        assertFalse(isViewInContainer(mContainerView, anchorView1));
        assertFalse(isViewInContainer(mContainerView, anchorView2));
        assertTrue(isViewInContainer(updatedContainerView, anchorView1));
        assertTrue(isViewInContainer(updatedContainerView, anchorView2));
        assertTrue(areEqual(layoutParams1, anchorView1.getLayoutParams()));
        assertTrue(areEqual(layoutParams2, anchorView2.getLayoutParams()));
    }

    private View addAnchorView() {
        View anchorView = mViewDelegate.acquireView();
        assertTrue(isViewInContainer(mContainerView, anchorView));
        return anchorView;
    }

    private void removeAnchorView(View anchorView) {
        mViewDelegate.removeView(anchorView);
        assertFalse(isViewInContainer(mContainerView, anchorView));
    }

    private LayoutParams setLayoutParams(View anchorView, int coords, int dimension) {
        float scale = DisplayAndroid.getNonMultiDisplay(mContainerView.getContext()).getDipScale();
        mViewDelegate.setViewPosition(
                anchorView, coords, coords, dimension, dimension, scale, 10, 10);
        return anchorView.getLayoutParams();
    }

    private FrameLayout updateContainerView() throws InterruptedException {
        FrameLayout containerView  = new FrameLayout(getActivity());
        getActivity().addView(containerView);
        mViewDelegate.updateCurrentContainerView(containerView,
                DisplayAndroid.getNonMultiDisplay(mContainerView.getContext()));
        return containerView;
    }

    private static void verifyAnchorViewCorrectlyTransferred(FrameLayout containerView,
            View anchorView, FrameLayout updatedContainerView, LayoutParams expectedParams) {
        assertTrue(areEqual(expectedParams, anchorView.getLayoutParams()));
        verifyAnchorViewCorrectlyTransferred(containerView, anchorView, updatedContainerView);
    }

    private static void verifyAnchorViewCorrectlyTransferred(FrameLayout containerView,
            View anchorView, FrameLayout updatedContainerView) {
        assertFalse(isViewInContainer(containerView, anchorView));
        assertTrue(isViewInContainer(updatedContainerView, anchorView));
        assertSame(anchorView, updatedContainerView.getChildAt(0));
    }

    private static boolean areEqual(LayoutParams params1, LayoutParams params2) {
        return params1.height == params2.height && params1.width == params2.width;
    }

    private static boolean isViewInContainer(FrameLayout containerView, View view) {
        return containerView.indexOfChild(view) != -1;
    }
}
