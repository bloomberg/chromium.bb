// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import org.chromium.android_webview.AwViewAndroidDelegate;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.gfx.DeviceDisplayInfo;

/**
 * Tests anchor views are correctly added/removed when their container view is updated.
 */
public class AwContentsAnchorViewTest extends AwTestBase {

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    @Feature({"AndroidWebView"})
    @SmallTest
    @UiThreadTest
    public void testMovedAndRemovedAnchorViewIsNotTransferred() throws Throwable {
        // Add, move and remove anchorView
        AwTestContainerView containerView = createAwTestContainerViewOnMainSync(mContentsClient);
        View anchorView = addAnchorView(containerView);
        setLayoutParams(containerView, anchorView, 1, 2);
        removeAnchorView(containerView, anchorView);

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView(containerView);

        // Verify that no anchor view is transferred between containerViews
        assertFalse(isViewInContainer(containerView, anchorView));
        assertFalse(isViewInContainer(updatedContainerView, anchorView));
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @UiThreadTest
    public void testTransferAnchorView() throws Throwable {
        // Add anchor view
        AwTestContainerView containerView = createAwTestContainerViewOnMainSync(mContentsClient);
        View anchorView = addAnchorView(containerView);
        LayoutParams layoutParams = anchorView.getLayoutParams();

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView(containerView);
        verifyAnchorViewCorrectlyTransferred(
                containerView, anchorView, updatedContainerView, layoutParams);
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @UiThreadTest
    public void testTransferMovedAnchorView() throws Throwable {
        // Add anchor view and move it
        AwTestContainerView containerView = createAwTestContainerViewOnMainSync(mContentsClient);
        View anchorView = addAnchorView(containerView);
        LayoutParams layoutParams = setLayoutParams(containerView, anchorView, 1, 2);

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView(containerView);

        verifyAnchorViewCorrectlyTransferred(
                containerView, anchorView, updatedContainerView, layoutParams);
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @UiThreadTest
    public void testRemoveTransferedAnchorView() throws Throwable {
        // Add anchor view
        AwTestContainerView containerView = createAwTestContainerViewOnMainSync(mContentsClient);
        View anchorView = addAnchorView(containerView);

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView(containerView);

        verifyAnchorViewCorrectlyTransferred(containerView, anchorView, updatedContainerView);

        // Remove transferred anchor view
        removeAnchorView(containerView, anchorView);
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @UiThreadTest
    public void testMoveTransferedAnchorView() throws Throwable {
        // Add anchor view
        AwTestContainerView containerView = createAwTestContainerViewOnMainSync(mContentsClient);
        View anchorView = addAnchorView(containerView);
        LayoutParams layoutParams = anchorView.getLayoutParams();

        // Replace container view
        FrameLayout updatedContainerView = updateContainerView(containerView);

        verifyAnchorViewCorrectlyTransferred(
                containerView, anchorView, updatedContainerView, layoutParams);

        // Move transferred anchor view
        assertFalse(areEqual(layoutParams, setLayoutParams(containerView, anchorView, 1, 2)));
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    @UiThreadTest
    public void testTransferMultipleMovedAnchorViews() throws Throwable {
        // Add and move anchorView1
        AwTestContainerView containerView = createAwTestContainerViewOnMainSync(mContentsClient);
        View anchorView1 = addAnchorView(containerView);
        LayoutParams layoutParams1 = setLayoutParams(containerView, anchorView1, 1, 2);

        // Add and move anchorView2
        View anchorView2 = addAnchorView(containerView);
        LayoutParams layoutParams2 = setLayoutParams(containerView, anchorView2, 2, 4);

        // Replace containerView
        FrameLayout updatedContainerView = updateContainerView(containerView);

        // Verify that anchor views are transfered with the same layout params.
        assertFalse(isViewInContainer(containerView, anchorView1));
        assertFalse(isViewInContainer(containerView, anchorView2));
        assertTrue(isViewInContainer(updatedContainerView, anchorView1));
        assertTrue(isViewInContainer(updatedContainerView, anchorView2));
        assertTrue(areEqual(layoutParams1, anchorView1.getLayoutParams()));
        assertTrue(areEqual(layoutParams2, anchorView2.getLayoutParams()));
    }

    private static View addAnchorView(AwTestContainerView containerView) {
        View anchorView = getViewDelegate(containerView).acquireView();
        assertTrue(isViewInContainer(containerView, anchorView));
        return anchorView;
    }

    private static void removeAnchorView(AwTestContainerView containerView, View anchorView) {
        getViewDelegate(containerView).removeView(anchorView);
        assertFalse(isViewInContainer(containerView, anchorView));
    }

    private static LayoutParams setLayoutParams(AwTestContainerView containerView, View anchorView,
                int coords, int dimension) {
        ViewAndroidDelegate delegate = getViewDelegate(containerView);
        float scale = (float) DeviceDisplayInfo.create(containerView.getContext()).getDIPScale();
        delegate.setViewPosition(anchorView, coords, coords, dimension, dimension, scale, 10, 10);
        return anchorView.getLayoutParams();
    }

    private FrameLayout updateContainerView(AwTestContainerView oldContainerView) {
        FrameLayout containerView  = new FrameLayout(getActivity());
        getActivity().addView(containerView);
        AwViewAndroidDelegate delegate = (AwViewAndroidDelegate) getViewDelegate(oldContainerView);
        delegate.updateCurrentContainerView(containerView);
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

    private static ViewAndroidDelegate getViewDelegate(AwTestContainerView containerView) {
        return containerView.getAwContents().getContentViewCore().getViewAndroidDelegate();
    }
}
