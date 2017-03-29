// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.os.SystemClock;
import android.support.test.filters.SmallTest;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.input.ImeAdapter;
import org.chromium.content.browser.input.TestImeAdapterDelegate;
import org.chromium.content.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Tests for PopupZoomer.
 */
public class PopupZoomerTest extends ContentShellTestBase {
    private CustomCanvasPopupZoomer mPopupZoomer;
    private ContentViewCore mContentViewCore;

    private static class CustomCanvasPopupZoomer extends PopupZoomer {
        Canvas mCanvas;
        long mPendingDraws = 0;

        CustomCanvasPopupZoomer(Context context, Canvas c) {
            super(context);
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

        public void finishPendingDraws() {
            // Finish all pending draw calls. A draw call may change mPendingDraws.
            while (mPendingDraws > 0) {
                onDraw(mCanvas);
            }
        }

    }

    private CustomCanvasPopupZoomer createPopupZoomerForTest(Context context) {
        return new CustomCanvasPopupZoomer(
                context, new Canvas(Bitmap.createBitmap(100, 100, Bitmap.Config.ALPHA_8)));
    }

    private void sendSingleTapTouchEventOnView(View view, float x, float y) {
        final long downEvent = SystemClock.uptimeMillis();
        view.onTouchEvent(
                MotionEvent.obtain(downEvent, downEvent, MotionEvent.ACTION_DOWN, x, y, 0));
        view.onTouchEvent(
                MotionEvent.obtain(downEvent, downEvent + 10, MotionEvent.ACTION_UP, x, y, 0));
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mPopupZoomer = createPopupZoomerForTest(getInstrumentation().getTargetContext());
        mContentViewCore = new ContentViewCore(getActivity(), "");
        ImeAdapter imeAdapter = new ImeAdapter(new TestInputMethodManagerWrapper(mContentViewCore),
                new TestImeAdapterDelegate(getContentViewCore().getContainerView()));
        mContentViewCore.setSelectionPopupControllerForTesting(
                new SelectionPopupController(getActivity(), null, null, null,
                        mContentViewCore.getRenderCoordinates(), imeAdapter));
        mContentViewCore.setPopupZoomerForTest(mPopupZoomer);
        mContentViewCore.setImeAdapterForTest(imeAdapter);
    }

    @SmallTest
    @Feature({"Navigation"})
    public void testDefaultCreateState() throws Exception {
        assertEquals(View.INVISIBLE, mPopupZoomer.getVisibility());
        assertFalse(mPopupZoomer.isShowing());
    }

    @SmallTest
    @Feature({"Navigation"})
    public void testShowWithoutBitmap() throws Exception {
        mPopupZoomer.show(new Rect(0, 0, 5, 5));

        // The view should be invisible.
        assertEquals(View.INVISIBLE, mPopupZoomer.getVisibility());
        assertFalse(mPopupZoomer.isShowing());
    }

    @SmallTest
    @Feature({"Navigation"})
    public void testShowWithBitmap() throws Exception {
        mPopupZoomer.setBitmap(Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8));
        mPopupZoomer.show(new Rect(0, 0, 5, 5));

        // The view should become visible.
        assertEquals(View.VISIBLE, mPopupZoomer.getVisibility());
        assertTrue(mPopupZoomer.isShowing());
    }

    @SmallTest
    @Feature({"Navigation"})
    public void testHide() throws Exception {
        mPopupZoomer.setBitmap(Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8));
        mPopupZoomer.show(new Rect(0, 0, 5, 5));

        // The view should become visible.
        assertEquals(View.VISIBLE, mPopupZoomer.getVisibility());
        assertTrue(mPopupZoomer.isShowing());

        // Call hide without animation.
        mPopupZoomer.hide(false);

        // The view should be invisible.
        assertEquals(View.INVISIBLE, mPopupZoomer.getVisibility());
        assertFalse(mPopupZoomer.isShowing());
    }

    @SmallTest
    @Feature({"Navigation"})
    public void testOnTouchEventOutsidePopup() throws Exception {
        mPopupZoomer.setBitmap(Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8));
        mPopupZoomer.show(new Rect(0, 0, 5, 5));

        // Wait for the show animation to finish.
        mPopupZoomer.finishPendingDraws();

        // The view should be visible.
        assertEquals(View.VISIBLE, mPopupZoomer.getVisibility());
        assertTrue(mPopupZoomer.isShowing());

        // Send tap event at a point outside the popup.
        // i.e. coordinates greater than 10 + PopupZoomer.ZOOM_BOUNDS_MARGIN
        sendSingleTapTouchEventOnView(mPopupZoomer, 50, 50);

        // Wait for the hide animation to finish.
        mPopupZoomer.finishPendingDraws();

        // The view should be invisible.
        assertEquals(View.INVISIBLE, mPopupZoomer.getVisibility());
        assertFalse(mPopupZoomer.isShowing());
    }

    @SmallTest
    @Feature({"Navigation"})
    public void testOnTouchEventInsidePopupNoOnTapListener() throws Exception {
        mPopupZoomer.setBitmap(Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8));
        mPopupZoomer.show(new Rect(0, 0, 5, 5));

        // Wait for the animation to finish.
        mPopupZoomer.finishPendingDraws();

        // The view should be visible.
        assertEquals(View.VISIBLE, mPopupZoomer.getVisibility());
        assertTrue(mPopupZoomer.isShowing());

        // Send tap event at a point inside the popup.
        // i.e. coordinates between PopupZoomer.ZOOM_BOUNDS_MARGIN and
        // PopupZoomer.ZOOM_BOUNDS_MARGIN + 10
        sendSingleTapTouchEventOnView(mPopupZoomer, 30, 30);

        // Wait for the animation to finish (if there is any).
        mPopupZoomer.finishPendingDraws();

        // The view should still be visible as no OnTapListener is set.
        assertEquals(View.VISIBLE, mPopupZoomer.getVisibility());
        assertTrue(mPopupZoomer.isShowing());
    }

    @SmallTest
    @Feature({"Navigation"})
    public void testHidePopupOnLosingFocus() throws Exception {
        mPopupZoomer.setBitmap(
                        Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8));
        mPopupZoomer.show(new Rect(0, 0, 5, 5));

        // Wait for the animation to finish.
        mPopupZoomer.finishPendingDraws();

        // The view should be visible.
        assertEquals(View.VISIBLE, mPopupZoomer.getVisibility());
        assertTrue(mPopupZoomer.isShowing());

        // Simulate losing the focus.
        mContentViewCore.onFocusChanged(false, true);

        // Wait for the hide animation to finish.
        mPopupZoomer.finishPendingDraws();

        // Now that another view has been focused, the view should be invisible.
        assertEquals(View.INVISIBLE, mPopupZoomer.getVisibility());
        assertFalse(mPopupZoomer.isShowing());
    }
}
