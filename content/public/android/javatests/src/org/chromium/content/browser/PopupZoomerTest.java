// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.input.ImeAdapter;
import org.chromium.content.browser.input.TextSuggestionHost;
import org.chromium.content.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

/**
 * Tests for PopupZoomer.
 */
@RunWith(ContentJUnit4ClassRunner.class)
public class PopupZoomerTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private CustomCanvasPopupZoomer mPopupZoomer;
    private ContentViewCore mContentViewCore;

    private static class CustomCanvasPopupZoomer extends PopupZoomer {
        Canvas mCanvas;
        long mPendingDraws = 0;

        CustomCanvasPopupZoomer(
                Context context, WebContents webContents, ViewGroup containerView, Canvas c) {
            super(context, webContents, containerView);
            mCanvas = c;
        }

        @Override
        public void invalidate() {
            mPendingDraws++;
        }

        @Override
        public void onDraw(Canvas c) {
            mPendingDraws--;
            super.onDraw(c);
        }

        // Test doesn't attach PopupZoomer to the view hierarchy,
        // but onDraw() should still go on.
        @Override
        protected boolean acceptZeroSizeView() {
            return true;
        }

        @Override
        protected void initOptionalListeners(final ViewGroup containerView) {}

        public void finishPendingDraws() {
            // Finish all pending draw calls. A draw call may change mPendingDraws.
            while (mPendingDraws > 0) {
                onDraw(mCanvas);
            }
        }
    }

    private CustomCanvasPopupZoomer createPopupZoomerForTest(
            Context context, WebContents webContents, ViewGroup containerView) {
        return new CustomCanvasPopupZoomer(context, webContents, containerView,
                new Canvas(Bitmap.createBitmap(100, 100, Bitmap.Config.ALPHA_8)));
    }

    private void sendSingleTapTouchEventOnView(View view, float x, float y) {
        final long downEvent = SystemClock.uptimeMillis();
        view.onTouchEvent(
                MotionEvent.obtain(downEvent, downEvent, MotionEvent.ACTION_DOWN, x, y, 0));
        view.onTouchEvent(
                MotionEvent.obtain(downEvent, downEvent + 10, MotionEvent.ACTION_UP, x, y, 0));
    }

    @Before
    public void setUp() throws Throwable {
        mActivityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Context context = mActivityTestRule.getActivity();
                WebContents webContents = mActivityTestRule.getContentViewCore().getWebContents();
                mContentViewCore = new ContentViewCore(context, "");
                mContentViewCore.setSelectionPopupControllerForTesting(
                        new SelectionPopupController(context, null, webContents, null));
                ViewGroup containerView = mActivityTestRule.getContentViewCore().getContainerView();
                mContentViewCore.setImeAdapterForTest(new ImeAdapter(webContents, containerView,
                        new TestInputMethodManagerWrapper(mContentViewCore)));
                mPopupZoomer = createPopupZoomerForTest(
                        InstrumentationRegistry.getTargetContext(), webContents, containerView);
                mContentViewCore.setPopupZoomerForTest(mPopupZoomer);
                mContentViewCore.setTextSuggestionHostForTesting(
                        new TextSuggestionHost(mActivityTestRule.getContentViewCore()));
            }
        });
    }

    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testDefaultCreateState() throws Exception {
        Assert.assertEquals(View.INVISIBLE, mPopupZoomer.getVisibility());
        Assert.assertFalse(mPopupZoomer.isShowing());
    }

    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testShowWithoutBitmap() throws Exception {
        mPopupZoomer.show(new Rect(0, 0, 5, 5));

        // The view should be invisible.
        Assert.assertEquals(View.INVISIBLE, mPopupZoomer.getVisibility());
        Assert.assertFalse(mPopupZoomer.isShowing());
    }

    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testShowWithBitmap() throws Exception {
        mPopupZoomer.setBitmap(Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8));
        mPopupZoomer.show(new Rect(0, 0, 5, 5));

        // The view should become visible.
        Assert.assertEquals(View.VISIBLE, mPopupZoomer.getVisibility());
        Assert.assertTrue(mPopupZoomer.isShowing());
    }

    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testHide() throws Exception {
        mPopupZoomer.setBitmap(Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8));
        mPopupZoomer.show(new Rect(0, 0, 5, 5));

        // The view should become visible.
        Assert.assertEquals(View.VISIBLE, mPopupZoomer.getVisibility());
        Assert.assertTrue(mPopupZoomer.isShowing());

        // Call hide without animation.
        mPopupZoomer.hide(false);

        // The view should be invisible.
        Assert.assertEquals(View.INVISIBLE, mPopupZoomer.getVisibility());
        Assert.assertFalse(mPopupZoomer.isShowing());
    }

    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testOnTouchEventOutsidePopup() throws Exception {
        mPopupZoomer.setBitmap(Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8));
        mPopupZoomer.show(new Rect(0, 0, 5, 5));

        // Wait for the show animation to finish.
        mPopupZoomer.finishPendingDraws();

        // The view should be visible.
        Assert.assertEquals(View.VISIBLE, mPopupZoomer.getVisibility());
        Assert.assertTrue(mPopupZoomer.isShowing());

        // Send tap event at a point outside the popup.
        // i.e. coordinates greater than 10 + PopupZoomer.ZOOM_BOUNDS_MARGIN
        sendSingleTapTouchEventOnView(mPopupZoomer, 50, 50);

        // Wait for the hide animation to finish.
        mPopupZoomer.finishPendingDraws();

        // The view should be invisible.
        Assert.assertEquals(View.INVISIBLE, mPopupZoomer.getVisibility());
        Assert.assertFalse(mPopupZoomer.isShowing());
    }

    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testOnTouchEventInsidePopupNoOnTapListener() throws Exception {
        mPopupZoomer.setBitmap(Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8));
        mPopupZoomer.show(new Rect(0, 0, 5, 5));

        // Wait for the animation to finish.
        mPopupZoomer.finishPendingDraws();

        // The view should be visible.
        Assert.assertEquals(View.VISIBLE, mPopupZoomer.getVisibility());
        Assert.assertTrue(mPopupZoomer.isShowing());

        // Send tap event at a point inside the popup.
        // i.e. coordinates between PopupZoomer.ZOOM_BOUNDS_MARGIN and
        // PopupZoomer.ZOOM_BOUNDS_MARGIN + 10
        sendSingleTapTouchEventOnView(mPopupZoomer, 30, 30);

        // Wait for the animation to finish (if there is any).
        mPopupZoomer.finishPendingDraws();

        // The view should still be visible as no OnTapListener is set.
        Assert.assertEquals(View.VISIBLE, mPopupZoomer.getVisibility());
        Assert.assertTrue(mPopupZoomer.isShowing());
    }

    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testHidePopupOnLosingFocus() throws Exception {
        mPopupZoomer.setBitmap(
                        Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8));
        mPopupZoomer.show(new Rect(0, 0, 5, 5));

        // Wait for the animation to finish.
        mPopupZoomer.finishPendingDraws();

        // The view should be visible.
        Assert.assertEquals(View.VISIBLE, mPopupZoomer.getVisibility());
        Assert.assertTrue(mPopupZoomer.isShowing());

        // Simulate losing the focus.
        mContentViewCore.onFocusChanged(false, true);

        // Wait for the hide animation to finish.
        mPopupZoomer.finishPendingDraws();

        // Now that another view has been focused, the view should be invisible.
        Assert.assertEquals(View.INVISIBLE, mPopupZoomer.getVisibility());
        Assert.assertFalse(mPopupZoomer.isShowing());
    }
}
