// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.test.suitebuilder.annotation.MediumTest;
import android.text.Editable;
import android.text.Selection;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TouchUtils;
import org.chromium.content_shell.ContentShellTestBase;

public class InsertionHandleTest extends ContentShellTestBase {

    private static final String FILENAME = "content/insertion_handle/editable_long_text.html";
    private static final String INPUT_TEXT_FILENAME = "content/insertion_handle/input_text.html";
    private static final String INPUT_TEXT_ID = "input_text";
    // Offset to compensate for the fact that the handle is below the text.
    private static final int VERTICAL_OFFSET = 10;
    // These positions should both be in the text area and not within
    // HANDLE_POSITON_TOLERANCE of each other in either coordinate.
    private static final int INITIAL_CLICK_X = 100;
    private static final int INITIAL_CLICK_Y = 100;
    private static final int DRAG_TO_X = 287;
    private static final int DRAG_TO_Y = 199;
    private static final int HANDLE_POSITION_TOLERANCE = 40;
    private static final String PASTE_TEXT = "**test text to paste**";

    private void dragHandleTo(int dragToX, int dragToY, int steps) throws Throwable {
        InsertionHandleController ihc = getContentViewCore().getInsertionHandleControllerForTest();
        HandleView handle = ihc.getHandleViewForTest();
        int initialX = handle.getPositionX();
        int initialY = handle.getPositionY() + VERTICAL_OFFSET;
        ContentView view = getContentView();

        int fromLocation[] = TouchUtils.getAbsoluteLocationFromRelative(view, initialX, initialY);
        int toLocation[] = TouchUtils.getAbsoluteLocationFromRelative(view, dragToX, dragToY);

        long downTime = TouchUtils.dragStart(getInstrumentation(), fromLocation[0],
                fromLocation[1]);
        assertWaitForHandleDraggingEquals(true);
        TouchUtils.dragTo(getInstrumentation(), fromLocation[0], toLocation[0], fromLocation[1],
                toLocation[1], steps, downTime);
        TouchUtils.dragEnd(getInstrumentation(), toLocation[0], toLocation[1], downTime);
        assertWaitForHandleDraggingEquals(false);
    }

    private void dragHandleTo(int dragToX, int dragToY) throws Throwable {
        dragHandleTo(dragToX, dragToY, 5);
    }

    private void assertWaitForHandleDraggingEquals(final boolean expected) throws Throwable {
        InsertionHandleController ihc = getContentViewCore().getInsertionHandleControllerForTest();
        final HandleView handle = ihc.getHandleViewForTest();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return handle.isDragging() == expected;
            }
        }));
    }

    @MediumTest
    @Feature({"TextSelection", "TextInput", "Main"})
    public void testDragInsertionHandle() throws Throwable {
        startActivityWithTestUrl(FILENAME);

        clickToShowInsertionHandle();

        dragHandleTo(DRAG_TO_X, DRAG_TO_Y);
        assertWaitForHandleNear(DRAG_TO_X, DRAG_TO_Y);

        // Unselecting should cause the handle to disappear.
        getImeAdapter().unselect();
        assertTrue(waitForHandleShowingEquals(false));
    }

    private static boolean isHandleNear(HandleView handle, int x, int y) {
        return (Math.abs(handle.getPositionX() - x) < HANDLE_POSITION_TOLERANCE) &&
                (Math.abs(handle.getPositionY() - y) < HANDLE_POSITION_TOLERANCE);
    }

    private void assertWaitForHandleNear(final int x, final int y) throws Throwable {
        InsertionHandleController ihc = getContentViewCore().getInsertionHandleControllerForTest();
        final HandleView handle = ihc.getHandleViewForTest();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return isHandleNear(handle, x, y);
            }
        }));
    }

    private boolean waitForHandleShowingEquals(final boolean shouldBeShowing)
            throws Throwable {
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                InsertionHandleController ihc =
                        getContentViewCore().getInsertionHandleControllerForTest();
                boolean isShowing = ihc != null && ihc.isShowing();
                return isShowing == shouldBeShowing;
            }
        });
    }

    @MediumTest
    @Feature({"TextSelection", "TextInput"})
    public void testPasteAtInsertionHandle() throws Throwable {
        startActivityWithTestUrl(FILENAME);

        clickToShowInsertionHandle();

        assertTrue(waitForHasSelectionPosition());

        // See below. Removed assignments to satisfy findbugs.
        // int offset = getSelectionStart();
        // String initialText = getEditableText();

        saveToClipboard(PASTE_TEXT);
        pasteOnMainSync();

        // TODO(aurimas): The ImeAdapter is often not receiving the correct update from the paste,
        // and so the editableText is never equal to the expectedText. Enable this check when fixed.
        // http://crbug.com/163966
        // String expectedText =
        //        initialText.substring(0, offset) + PASTE_TEXT + initialText.substring(offset);
        // assertTrue(waitForEditableTextEquals(expectedText));

        assertTrue(waitForHandleShowingEquals(false));
    }

    private boolean waitForEditableTextEquals(final String expectedText)
            throws Throwable {
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getEditableText().trim().equals(expectedText.trim());
            }
        });
    }

    private boolean waitForHasSelectionPosition()
            throws Throwable {
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                int start = getSelectionStart();
                int end = getSelectionEnd();
                return start > 0 && start == end;
            }
        });
    }

    private void pasteOnMainSync() {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().getInsertionHandleControllerForTest().paste();
            }
        });
    }

    @MediumTest
    @Feature({"TextSelection", "TextInput", "Main"})
    public void testDragInsertionHandleInputText() throws Throwable {
        startActivityWithTestUrl(INPUT_TEXT_FILENAME);

        DOMUtils.clickNode(this, getContentView(),
                new TestCallbackHelperContainer(getContentView()), INPUT_TEXT_ID);
        TouchUtils.sleepForDoubleTapTimeout(getInstrumentation());
        DOMUtils.clickNode(this, getContentView(),
                new TestCallbackHelperContainer(getContentView()), INPUT_TEXT_ID);

        assertTrue(waitForHandleShowingEquals(true));
        assertWaitForZoomFinished();
        assertTrue(waitForHandleViewStopped());

        InsertionHandleController ihc = getContentViewCore().getInsertionHandleControllerForTest();
        HandleView handle = ihc.getHandleViewForTest();

        int initialX = handle.getPositionX();
        int initialY = handle.getPositionY() + VERTICAL_OFFSET;
        int dragToX = initialX + 120;
        int dragToY = initialY;
        dragHandleTo(dragToX, dragToY);

        assertTrue(waitForHandleViewStopped());
        assertWaitForHandleNear(dragToX, initialY);

        TouchUtils.sleepForDoubleTapTimeout(getInstrumentation());

        initialX = handle.getPositionX();
        initialY = handle.getPositionY() + VERTICAL_OFFSET;
        dragToX = initialX - 120;
        dragToY = initialY;
        dragHandleTo(dragToX, dragToY);

        assertTrue(waitForHandleViewStopped());
        // Vertical drag should not change the y-position.
        assertWaitForHandleNear(dragToX, initialY);
    }

    @MediumTest
    @Feature({"TextSelection", "TextInput", "Main"})
    public void testDragInsertionHandleInputTextOutsideBounds() throws Throwable {
        startActivityWithTestUrl(INPUT_TEXT_FILENAME);

        DOMUtils.clickNode(this, getContentView(),
                new TestCallbackHelperContainer(getContentView()), INPUT_TEXT_ID);
        TouchUtils.sleepForDoubleTapTimeout(getInstrumentation());
        DOMUtils.clickNode(this, getContentView(),
                new TestCallbackHelperContainer(getContentView()), INPUT_TEXT_ID);

        assertTrue(waitForHandleShowingEquals(true));
        assertWaitForZoomFinished();

        InsertionHandleController ihc = getContentViewCore().getInsertionHandleControllerForTest();
        HandleView handle = ihc.getHandleViewForTest();

        getContentView().zoomReset();
        assertWaitForZoomFinished();

        // Quickly (i.e. few move events) drag the handle down and to the right. When this happens,
        // the handle should stay vertically aligned with the drag position.
        int initialX = handle.getPositionX();
        int initialY = handle.getPositionY() + VERTICAL_OFFSET;
        int dragToX = initialX;
        int dragToY = initialY + 150;

        // A vertical drag should not move the insertion handle.
        dragHandleTo(dragToX, dragToY);
        assertTrue(waitForHandleViewStopped());
        // TODO(cjhopman): This currently does not work, dragging above or below will snap to the
        // beginning/end of the editable. See http://crbug.com/169055
        //assertWaitForHandleNear(initialX, initialY);

        // The input box does not go to the edge of the screen, and neither should the insertion
        // handle.
        dragToX = getContentView().getWidth();
        dragHandleTo(dragToX, dragToY);
        assertTrue(waitForHandleViewStopped());
        assertTrue(handle.getPositionX() < dragToX - 100);
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        // No way to just clear clipboard, so setting to empty string instead.
        saveToClipboard("");
    }

    private void clickToShowInsertionHandle() throws Throwable {
        TouchUtils.singleClickView(getInstrumentation(), getContentView(), INITIAL_CLICK_X,
                INITIAL_CLICK_Y);
        TouchUtils.sleepForDoubleTapTimeout(getInstrumentation());
        TouchUtils.singleClickView(getInstrumentation(), getContentView(), INITIAL_CLICK_X,
                INITIAL_CLICK_Y);
        assertTrue(waitForHandleShowingEquals(true));
        assertWaitForZoomFinished();
    }

    private void assertWaitForZoomFinished() throws Throwable {
        // If the polling interval is too short, slowly zooming may be detected as not zooming.
        final int POLLING_INTERVAL = 200;
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            float mScale = -1;
            @Override
            public boolean isSatisfied() {
                float lastScale = mScale;
                mScale = getContentView().getScale();
                return mScale == lastScale;
            }
        }, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL, POLLING_INTERVAL));
    }

    private boolean waitForHandleViewStopped() throws Throwable {
        // If the polling interval is too short, slowly zooming may be detected as not zooming.
        final int POLLING_INTERVAL = 200;
        return CriteriaHelper.pollForCriteria(new Criteria() {
            int mPositionX = -1;
            int mPositionY = -1;
            @Override
            public boolean isSatisfied() {
                int lastPositionX = mPositionX;
                int lastPositionY = mPositionY;
                InsertionHandleController ihc =
                        getContentViewCore().getInsertionHandleControllerForTest();
                HandleView handle = ihc.getHandleViewForTest();
                mPositionX = handle.getPositionX();
                mPositionY = handle.getPositionY();
                return !handle.isDragging() &&
                        mPositionX == lastPositionX && mPositionY == lastPositionY;
            }
        }, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL, POLLING_INTERVAL);
    }

    private int getSelectionStart() {
        return Selection.getSelectionStart(getEditable());
    }

    private int getSelectionEnd() {
        return Selection.getSelectionEnd(getEditable());
    }

    private Editable getEditable() {
        return getContentViewCore().getEditableForTest();
    }

    private String getEditableText() {
        return getEditable().toString();
    }

    private void saveToClipboard(String text) {
        ClipboardManager clipMgr =
                (ClipboardManager) getActivity().getSystemService(Context.CLIPBOARD_SERVICE);
        clipMgr.setPrimaryClip(ClipData.newPlainText(null, text));
    }

    private ImeAdapter getImeAdapter() {
        return getContentViewCore().getImeAdapterForTest();
    }
}
