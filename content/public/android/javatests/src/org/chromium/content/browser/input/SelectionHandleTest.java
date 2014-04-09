// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.os.SystemClock;
import android.test.FlakyTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.text.Editable;
import android.text.Selection;
import android.view.MotionEvent;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.RenderCoordinates;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.Callable;

/**
 * Tests for the selection handles that allow to select text in both editable and non-editable
 * elements.
 */
public class SelectionHandleTest extends ContentShellTestBase {
    private static final String META_DISABLE_ZOOM =
        "<meta name=\"viewport\" content=\"" +
        "height=device-height," +
        "width=device-width," +
        "initial-scale=1.0," +
        "minimum-scale=1.0," +
        "maximum-scale=1.0," +
        "\" />";

    // For these we use a tiny font-size so that we can be more strict on the expected handle
    // positions.
    private static final String TEXTAREA_ID = "textarea";
    private static final String TEXTAREA_DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head>" + META_DISABLE_ZOOM + "</head><body>" +
            "<textarea id=\"" + TEXTAREA_ID +
            "\" cols=\"40\" rows=\"20\" style=\"font-size:6px\">" +
            "L r m i s m d l r s t a e , c n e t t r a i i i i g e i , s d d e u m d t m o " +
            "i c d d n u l b r e d l r m g a l q a U e i a m n m e i m q i n s r d " +
            "e e c t t o u l m o a o i n s u a i u p x a o m d c n e u t D i a t " +
            "i u e o o i r p e e d r t n o u t t v l t s e i l m o o e u u i t u l " +
            "p r a u . x e t u s n o c e a c p d t t o p o d n , u t n u p q i " +
            "o f c a e e u t o l t n m d s l b r m." +
            "L r m i s m d l r s t a e , c n e t t r a i i i i g e i , s d d e u m d t m o " +
            "i c d d n u l b r e d l r m g a l q a U e i a m n m e i m q i n s r d " +
            "e e c t t o u l m o a o i n s u a i u p x a o m d c n e u t D i a t " +
            "i u e o o i r p e e d r t n o u t t v l t s e i l m o o e u u i t u l " +
            "p r a u . x e t u s n o c e a c p d t t o p o d n , u t n u p q i " +
            "o f c a e e u t o l t n m d s l b r m." +
            "</textarea>" +
            "</body></html>");

    private static final String NONEDITABLE_DIV_ID = "noneditable";
    private static final String NONEDITABLE_DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head>" + META_DISABLE_ZOOM + "</head><body>" +
            "<div id=\"" + NONEDITABLE_DIV_ID + "\" style=\"width:200; font-size:6px\">" +
            "L r m i s m d l r s t a e , c n e t t r a i i i i g e i , s d d e u m d t m o " +
            "i c d d n u l b r e d l r m g a l q a U e i a m n m e i m q i n s r d " +
            "e e c t t o u l m o a o i n s u a i u p x a o m d c n e u t D i a t " +
            "i u e o o i r p e e d r t n o u t t v l t s e i l m o o e u u i t u l " +
            "p r a u . x e t u s n o c e a c p d t t o p o d n , u t n u p q i " +
            "o f c a e e u t o l t n m d s l b r m." +
            "L r m i s m d l r s t a e , c n e t t r a i i i i g e i , s d d e u m d t m o " +
            "i c d d n u l b r e d l r m g a l q a U e i a m n m e i m q i n s r d " +
            "e e c t t o u l m o a o i n s u a i u p x a o m d c n e u t D i a t " +
            "i u e o o i r p e e d r t n o u t t v l t s e i l m o o e u u i t u l " +
            "p r a u . x e t u s n o c e a c p d t t o p o d n , u t n u p q i " +
            "o f c a e e u t o l t n m d s l b r m." +
            "</div>" +
            "</body></html>");

    // TODO(cjhopman): These tolerances should be based on the actual width/height of a
    // character/line.
    private static final int HANDLE_POSITION_X_TOLERANCE_PIX = 20;
    private static final int HANDLE_POSITION_Y_TOLERANCE_PIX = 30;

    private enum TestPageType {
        EDITABLE(TEXTAREA_ID, TEXTAREA_DATA_URL, true),
        NONEDITABLE(NONEDITABLE_DIV_ID, NONEDITABLE_DATA_URL, false);

        final String nodeId;
        final String dataUrl;
        final boolean selectionShouldBeEditable;

        TestPageType(String nodeId, String dataUrl, boolean selectionShouldBeEditable) {
            this.nodeId = nodeId;
            this.dataUrl = dataUrl;
            this.selectionShouldBeEditable = selectionShouldBeEditable;
        }
    }

    private void launchWithUrl(String url) throws Throwable {
        launchContentShellWithUrl(url);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
        assertWaitForPageScaleFactorMatch(1.0f);

        // The TestInputMethodManagerWrapper intercepts showSoftInput so that a keyboard is never
        // brought up.
        getImeAdapter().setInputMethodManagerWrapper(
                new TestInputMethodManagerWrapper(getContentViewCore()));
    }

    private void assertWaitForHasSelectionPosition()
            throws Throwable {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                int start = getSelectionStart();
                int end = getSelectionEnd();
                return start > 0 && start == end;
            }
        }));
    }

    /**
     * Verifies that when a long-press is performed on static page text,
     * selection handles appear and that handles can be dragged to extend the
     * selection. Does not check exact handle position as this will depend on
     * screen size; instead, position is expected to be correct within
     * HANDLE_POSITION_TOLERANCE_PIX.
     *
     * Test is flaky: crbug.com/290375
     * @MediumTest
     * @Feature({ "TextSelection", "Main" })
     */
    @FlakyTest
    public void testNoneditableSelectionHandles() throws Throwable {
        doSelectionHandleTest(TestPageType.NONEDITABLE);
    }

    /**
     * Verifies that when a long-press is performed on editable text (within a
     * textarea), selection handles appear and that handles can be dragged to
     * extend the selection. Does not check exact handle position as this will
     * depend on screen size; instead, position is expected to be correct within
     * HANDLE_POSITION_TOLERANCE_PIX.
     */
    @MediumTest
    @Feature({ "TextSelection" })
    public void testEditableSelectionHandles() throws Throwable {
        doSelectionHandleTest(TestPageType.EDITABLE);
    }

    /**
     * Verifies that the visibility of handles is correct when visible clipping
     * rectangle is set.
     */
    @MediumTest
    @Feature({ "TextSelection" })
    public void testEditableSelectionHandlesStartNotVisibleEndVisible() throws Throwable {
        doSelectionHandleTestVisibility(TestPageType.EDITABLE, false, true, new PointF(.5f, .5f),
                new PointF(0,1));
    }

    @MediumTest
    @Feature({ "TextSelection" })
    public void testEditableSelectionHandlesStartVisibleEndNotVisible() throws Throwable {
        doSelectionHandleTestVisibility(TestPageType.EDITABLE, true, false, new PointF(1,0),
                new PointF(.5f, .5f));
    }

    @MediumTest
    @Feature({ "TextSelection" })
    public void testEditableSelectionHandlesStartVisibleEndVisible() throws Throwable {
        doSelectionHandleTestVisibility(TestPageType.EDITABLE, true, true, new PointF(1, 0),
                new PointF(0,1));
    }

    @MediumTest
    @Feature({ "TextSelection" })
    public void testEditableSelectionHandlesStartNotVisibleEndNotVisible() throws Throwable {
        doSelectionHandleTestVisibility(TestPageType.EDITABLE, false, false, new PointF(1,0),
                new PointF(1, 0));
    }

    private void doSelectionHandleTestVisibility(TestPageType pageType,
            boolean startHandleVisible, boolean endHandleVisible,
            PointF affineTopLeft, PointF affineBottomRight) throws Throwable {
        launchWithUrl(pageType.dataUrl);

        clickNodeToShowSelectionHandles(pageType.nodeId);
        assertWaitForSelectionEditableEquals(pageType.selectionShouldBeEditable);

        HandleView startHandle = getStartHandle();
        HandleView endHandle = getEndHandle();

        Rect nodeWindowBounds = getNodeBoundsPix(pageType.nodeId);

        int visibleBoundsLeftX = Math.round(affineTopLeft.x * nodeWindowBounds.left
                + affineTopLeft.y * nodeWindowBounds.right);
        int visibleBoundsTopY = Math.round(affineTopLeft.x * nodeWindowBounds.top
                + affineTopLeft.y * nodeWindowBounds.bottom);

        int visibleBoundsRightX = Math.round(affineBottomRight.x * nodeWindowBounds.left
                + affineBottomRight.y * nodeWindowBounds.right);
        int visibleBoundsBottomY = Math.round(affineBottomRight.x * nodeWindowBounds.top
                + affineBottomRight.y * nodeWindowBounds.bottom);

        getContentViewCore().getSelectionHandleControllerForTest().setVisibleClippingRectangle(
                visibleBoundsLeftX, visibleBoundsTopY, visibleBoundsRightX, visibleBoundsBottomY);

        int leftX = (nodeWindowBounds.left + nodeWindowBounds.centerX()) / 2;
        int rightX = (nodeWindowBounds.right + nodeWindowBounds.centerX()) / 2;

        int topY = (nodeWindowBounds.top + nodeWindowBounds.centerY()) / 2;
        int bottomY = (nodeWindowBounds.bottom + nodeWindowBounds.centerY()) / 2;

        dragHandleAndCheckSelectionChange(startHandle, leftX, topY, -1, 0, startHandleVisible);
        dragHandleAndCheckSelectionChange(endHandle, rightX, bottomY, 0, 1, endHandleVisible);

        clickToDismissHandles();
    }

    private void doSelectionHandleTest(TestPageType pageType) throws Throwable {
        launchWithUrl(pageType.dataUrl);

        clickNodeToShowSelectionHandles(pageType.nodeId);
        assertWaitForSelectionEditableEquals(pageType.selectionShouldBeEditable);

        HandleView startHandle = getStartHandle();
        HandleView endHandle = getEndHandle();

        Rect nodeWindowBounds = getNodeBoundsPix(pageType.nodeId);

        int leftX = (nodeWindowBounds.left + nodeWindowBounds.centerX()) / 2;
        int centerX = nodeWindowBounds.centerX();
        int rightX = (nodeWindowBounds.right + nodeWindowBounds.centerX()) / 2;

        int topY = (nodeWindowBounds.top + nodeWindowBounds.centerY()) / 2;
        int centerY = nodeWindowBounds.centerY();
        int bottomY = (nodeWindowBounds.bottom + nodeWindowBounds.centerY()) / 2;

        // Drag start handle up and to the left. The selection start should decrease.
        dragHandleAndCheckSelectionChange(startHandle, leftX, topY, -1, 0, true);
        // Drag end handle down and to the right. The selection end should increase.
        dragHandleAndCheckSelectionChange(endHandle, rightX, bottomY, 0, 1, true);
        // Drag start handle back to the middle. The selection start should increase.
        dragHandleAndCheckSelectionChange(startHandle, centerX, centerY, 1, 0, true);
        // Drag end handle up and to the left past the start handle. Both selection start and end
        // should decrease.
        dragHandleAndCheckSelectionChange(endHandle, leftX, topY, -1, -1, true);
        // Drag start handle down and to the right past the end handle. Both selection start and end
        // should increase.
        dragHandleAndCheckSelectionChange(startHandle, rightX, bottomY, 1, 1, true);

        clickToDismissHandles();
    }

    private void dragHandleAndCheckSelectionChange(final HandleView handle,
            final int dragToX, final int dragToY,
            final int expectedStartChange, final int expectedEndChange,
            final boolean expectedHandleVisible) throws Throwable {
        String initialText = getContentViewCore().getSelectedText();
        final int initialSelectionEnd = getSelectionEnd();
        final int initialSelectionStart = getSelectionStart();

        dragHandleTo(handle, dragToX, dragToY, 10);
        assertWaitForEitherHandleNear(dragToX, dragToY);

        if (getContentViewCore().isSelectionEditable()) {
            assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    int startChange = getSelectionStart() - initialSelectionStart;
                    // TODO(cjhopman): Due to http://crbug.com/244633 we can't really assert that
                    // there is no change when we expect to be able to.
                    if (expectedStartChange != 0) {
                        if ((int) Math.signum(startChange) != expectedStartChange) return false;
                    }

                    int endChange = getSelectionEnd() - initialSelectionEnd;
                    if (expectedEndChange != 0) {
                        if ((int) Math.signum(endChange) != expectedEndChange) return false;
                    }

                    if (expectedHandleVisible != handle.isPositionVisible()) return false;

                    return true;
                }
            }));
        }

        assertWaitForHandleViewStopped(getStartHandle());
        assertWaitForHandleViewStopped(getEndHandle());
    }

    private void assertWaitForSelectionEditableEquals(final boolean expected) throws Throwable {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getContentViewCore().isSelectionEditable() == expected;
            }
        }));
    }

    private void assertWaitForHandleViewStopped(final HandleView handle) throws Throwable {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            private Point position = new Point(-1, -1);
            @Override
            public boolean isSatisfied() {
                Point lastPosition = position;
                position = getHandlePosition(handle);
                return !handle.isDragging() &&
                        position.equals(lastPosition);
            }
        }));
    }

    /**
     * Verifies that when a selection is made within static page text, that the
     * contextual action bar of the correct type is displayed. Also verified
     * that the bar disappears upon deselection.
     */
    @MediumTest
    @Feature({ "TextSelection" })
    public void testNoneditableSelectionActionBar() throws Throwable {
        doSelectionActionBarTest(TestPageType.NONEDITABLE);
    }

    /**
     * Verifies that when a selection is made within editable text, that the
     * contextual action bar of the correct type is displayed. Also verified
     * that the bar disappears upon deselection.
     */
    @MediumTest
    @Feature({ "TextSelection" })
    public void testEditableSelectionActionBar() throws Throwable {
        doSelectionActionBarTest(TestPageType.EDITABLE);
    }

    private void doSelectionActionBarTest(TestPageType pageType) throws Throwable {
        launchWithUrl(pageType.dataUrl);
        assertFalse(getContentViewCore().isSelectActionBarShowing());
        clickNodeToShowSelectionHandles(pageType.nodeId);
        assertWaitForSelectActionBarShowingEquals(true);
        clickToDismissHandles();
        assertWaitForSelectActionBarShowingEquals(false);
    }

    private static Point getHandlePosition(final HandleView handle) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Point>() {
            @Override
            public Point call() {
                return new Point(handle.getAdjustedPositionX(), handle.getAdjustedPositionY());
            }
        });
    }

    private static boolean isHandleNear(HandleView handle, int x, int y) {
        Point position = getHandlePosition(handle);
        return (Math.abs(position.x - x) < HANDLE_POSITION_X_TOLERANCE_PIX) &&
                (Math.abs(position.y - y) < HANDLE_POSITION_Y_TOLERANCE_PIX);
    }

    private void assertWaitForHandleNear(final HandleView handle, final int x, final int y)
            throws Throwable {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return isHandleNear(handle, x, y);
            }
        }));
    }

    private void assertWaitForEitherHandleNear(final int x, final int y) throws Throwable {
        final HandleView startHandle = getStartHandle();
        final HandleView endHandle = getEndHandle();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return isHandleNear(startHandle, x, y) || isHandleNear(endHandle, x, y);
            }
        }));
    }

    private void assertWaitForHandlesShowingEquals(final boolean shouldBeShowing) throws Throwable {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                SelectionHandleController shc =
                        getContentViewCore().getSelectionHandleControllerForTest();
                boolean isShowing = shc != null && shc.isShowing();
                return shouldBeShowing == isShowing;
            }
        }));
    }


    private void dragHandleTo(final HandleView handle, final int dragToX, final int dragToY,
            final int steps) throws Throwable {
        ContentView view = getContentView();
        assertTrue(ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                int adjustedX = handle.getAdjustedPositionX();
                int adjustedY = handle.getAdjustedPositionY();
                int realX = handle.getPositionX();
                int realY = handle.getPositionY();

                int realDragToX = dragToX + (realX - adjustedX);
                int realDragToY = dragToY + (realY - adjustedY);

                ContentView view = getContentView();
                int[] fromLocation = TestTouchUtils.getAbsoluteLocationFromRelative(
                        view, realX, realY);
                int[] toLocation = TestTouchUtils.getAbsoluteLocationFromRelative(
                        view, realDragToX, realDragToY);

                long downTime = SystemClock.uptimeMillis();
                MotionEvent event = MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN,
                        fromLocation[0], fromLocation[1], 0);
                handle.dispatchTouchEvent(event);

                if (!handle.isDragging()) return false;

                for (int i = 0; i < steps; i++) {
                    float scale = (float) (i + 1) / steps;
                    int x = fromLocation[0] + (int) (scale * (toLocation[0] - fromLocation[0]));
                    int y = fromLocation[1] + (int) (scale * (toLocation[1] - fromLocation[1]));
                    long eventTime = SystemClock.uptimeMillis();
                    event = MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_MOVE,
                            x, y, 0);
                    handle.dispatchTouchEvent(event);
                }
                long upTime = SystemClock.uptimeMillis();
                event = MotionEvent.obtain(downTime, upTime, MotionEvent.ACTION_UP,
                        toLocation[0], toLocation[1], 0);
                handle.dispatchTouchEvent(event);

                return !handle.isDragging();
            }
        }));
    }

    private Rect getNodeBoundsPix(String nodeId) throws Throwable {
        Rect nodeBounds = DOMUtils.getNodeBounds(getContentViewCore(),
                new TestCallbackHelperContainer(getContentView()), nodeId);

        RenderCoordinates renderCoordinates = getContentViewCore().getRenderCoordinates();
        int offsetX = getContentViewCore().getViewportSizeOffsetWidthPix();
        int offsetY = getContentViewCore().getViewportSizeOffsetHeightPix();

        int left = (int) renderCoordinates.fromLocalCssToPix(nodeBounds.left) + offsetX;
        int right = (int) renderCoordinates.fromLocalCssToPix(nodeBounds.right) + offsetX;
        int top = (int) renderCoordinates.fromLocalCssToPix(nodeBounds.top) + offsetY;
        int bottom = (int) renderCoordinates.fromLocalCssToPix(nodeBounds.bottom) + offsetY;

        return new Rect(left, top, right, bottom);
    }

    private void clickNodeToShowSelectionHandles(String nodeId) throws Throwable {
        Rect nodeWindowBounds = getNodeBoundsPix(nodeId);

        TouchCommon touchCommon = new TouchCommon(this);
        int centerX = nodeWindowBounds.centerX();
        int centerY = nodeWindowBounds.centerY();
        touchCommon.longPressView(getContentView(), centerX, centerY);

        assertWaitForHandlesShowingEquals(true);
        assertWaitForHandleViewStopped(getStartHandle());

        // No words wrap in the sample text so handles should be at the same y
        // position.
        assertEquals(getStartHandle().getPositionY(), getEndHandle().getPositionY());
    }

    private void clickToDismissHandles() throws Throwable {
        TestTouchUtils.sleepForDoubleTapTimeout(getInstrumentation());
        new TouchCommon(this).singleClickView(getContentView(), 0, 0);
        assertWaitForHandlesShowingEquals(false);
    }

    private void assertWaitForSelectActionBarShowingEquals(final boolean shouldBeShowing)
            throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return shouldBeShowing == getContentViewCore().isSelectActionBarShowing();
            }
        }));
    }

    public void assertWaitForHasInputConnection() {
        try {
            assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return getContentViewCore().getInputConnectionForTest() != null;
                }
            }));
        } catch (InterruptedException e) {
            fail();
        }
    }

    private ImeAdapter getImeAdapter() {
        return getContentViewCore().getImeAdapterForTest();
    }

    private int getSelectionStart() {
        return Selection.getSelectionStart(getEditable());
    }

    private int getSelectionEnd() {
        return Selection.getSelectionEnd(getEditable());
    }

    private Editable getEditable() {
        // We have to wait for the input connection (with the IME) to be created before accessing
        // the ContentViewCore's editable.
        assertWaitForHasInputConnection();
        return getContentViewCore().getEditableForTest();
    }

    private HandleView getStartHandle() {
        SelectionHandleController shc = getContentViewCore().getSelectionHandleControllerForTest();
        return shc.getStartHandleViewForTest();
    }

    private HandleView getEndHandle() {
        SelectionHandleController shc = getContentViewCore().getSelectionHandleControllerForTest();
        return shc.getEndHandleViewForTest();
    }
}
