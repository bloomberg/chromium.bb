// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.graphics.Rect;
import android.test.suitebuilder.annotation.MediumTest;
import android.text.Editable;
import android.text.Selection;
import android.view.KeyEvent;
import android.view.View;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.RenderCoordinates;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Tests the insertion handle that allows users to paste text.
 */
public class InsertionHandleTest extends ContentShellTestBase {
    private static final String META_DISABLE_ZOOM =
        "<meta name=\"viewport\" content=\"" +
        "height=device-height," +
        "width=device-width," +
        "initial-scale=1.0," +
        "minimum-scale=1.0," +
        "maximum-scale=1.0," +
        "\" />";

    private static final String TEXTAREA_ID = "textarea";
    private static final String TEXTAREA_DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head>" + META_DISABLE_ZOOM + "</head><body>" +
            "<textarea id=\"" + TEXTAREA_ID + "\" cols=\"20\" rows=\"10\">" +
            "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor " +
            "incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud " +
            "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute " +
            "irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla " +
            "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui " +
            "officia deserunt mollit anim id est laborum." +
            "</textarea>" +
            "</body></html>");

    private static final String INPUT_TEXT_ID = "input_text";
    private static final String INPUT_TEXT_DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head>" + META_DISABLE_ZOOM + "</head><body>" +
            "<input id=\"input_text\" type=\"text\" value=\"" +
            "T0D0(cjhopman): put amusing sample text here. Make sure it is at least " +
            "100 characters.  123456789012345678901234567890\" size=20></input>" +
            "</body></html>");

    // Offset to compensate for the fact that the handle is below the text.
    private static final int VERTICAL_OFFSET = 10;
    private static final int HANDLE_POSITION_TOLERANCE = 20;
    private static final String PASTE_TEXT = "**test text to paste**";


    public void launchWithUrl(String url) throws Throwable {
        launchContentShellWithUrl(url);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
        assertWaitForPageScaleFactorMatch(1.0f);

        // The TestInputMethodManagerWrapper intercepts showSoftInput so that a keyboard is never
        // brought up.
        getContentViewCore().getImeAdapterForTest().setInputMethodManagerWrapper(
                new TestInputMethodManagerWrapper(getContentViewCore()));
    }

    @MediumTest
    @Feature({"TextSelection", "TextInput", "Main"})
    public void testUnselectHidesHandle() throws Throwable {
        launchWithUrl(TEXTAREA_DATA_URL);
        clickNodeToShowInsertionHandle(TEXTAREA_ID);

        // Unselecting should cause the handle to disappear.
        unselectOnMainSync();
        assertTrue(waitForHandleShowingEquals(false));
    }

    @MediumTest
    @Feature({"TextSelection", "TextInput", "Main"})
    public void testUpdateContainerViewAndUnselectHidesHandle() throws Throwable {
        launchWithUrl(TEXTAREA_DATA_URL);
        replaceContainerView();

        clickNodeToShowInsertionHandle(TEXTAREA_ID);

        // Unselecting should cause the handle to disappear.
        unselectOnMainSync();
        assertTrue(waitForHandleShowingEquals(false));
    }

    /**
     * @MediumTest
     * @Feature({"TextSelection", "TextInput", "Main"})
     * http://crbug.com/169648
     */
    @DisabledTest
    public void testKeyEventHidesHandle() throws Throwable {
        launchWithUrl(TEXTAREA_DATA_URL);
        clickNodeToShowInsertionHandle(TEXTAREA_ID);

        getInstrumentation().sendKeySync(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_X));
        getInstrumentation().sendKeySync(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_X));
        assertTrue(waitForHandleShowingEquals(false));
    }

    /**
     * @MediumTest
     * @Feature({"TextSelection", "TextInput", "Main"})
     * http://crbug.com/169648
     */
    @DisabledTest
    public void testDragInsertionHandle() throws Throwable {
        launchWithUrl(TEXTAREA_DATA_URL);

        clickNodeToShowInsertionHandle(TEXTAREA_ID);

        InsertionHandleController ihc = getContentViewCore().getInsertionHandleControllerForTest();
        HandleView handle = ihc.getHandleViewForTest();

        int initialX = handle.getPositionX();
        int initialY = handle.getPositionY();
        int dragToX = initialX + 120;
        int dragToY = initialY + 120;

        dragHandleTo(dragToX, dragToY);
        assertWaitForHandleNear(dragToX, dragToY);
    }


    @MediumTest
    @Feature({"TextSelection", "TextInput", "Main"})
    public void testPasteAtInsertionHandle() throws Throwable {
        launchWithUrl(TEXTAREA_DATA_URL);

        clickNodeToShowInsertionHandle(TEXTAREA_ID);

        int offset = getSelectionStart();
        String initialText = getEditableText();

        saveToClipboard(PASTE_TEXT);
        pasteOnMainSync();

        String expectedText =
               initialText.substring(0, offset) + PASTE_TEXT + initialText.substring(offset);
        assertTrue(waitForEditableTextEquals(expectedText));
        assertTrue(waitForHandleShowingEquals(false));
    }

    /**
     * @MediumTest
     * @Feature({"TextSelection", "TextInput", "Main"})
     * http://crbug.com/169648
     */
    @DisabledTest
    public void testDragInsertionHandleInputText() throws Throwable {
        launchWithUrl(INPUT_TEXT_DATA_URL);

        clickNodeToShowInsertionHandle(INPUT_TEXT_ID);

        InsertionHandleController ihc = getContentViewCore().getInsertionHandleControllerForTest();
        HandleView handle = ihc.getHandleViewForTest();

        int initialX = handle.getPositionX();
        int initialY = handle.getPositionY();
        int dragToX = initialX + 120;
        int dragToY = initialY;
        dragHandleTo(dragToX, dragToY);
        assertWaitForHandleNear(dragToX, initialY);

        TestTouchUtils.sleepForDoubleTapTimeout(getInstrumentation());

        initialX = handle.getPositionX();
        initialY = handle.getPositionY();
        dragToY = initialY + 120;
        dragHandleTo(initialX, dragToY);
        // Vertical drag should not change the y-position.
        assertWaitForHandleNear(initialX, initialY);
    }

    /**
     * @MediumTest
     * @Feature({"TextSelection", "TextInput", "Main"})
     * http://crbug.com/169648
     */
    @DisabledTest
    public void testDragInsertionHandleInputTextOutsideBounds() throws Throwable {
        launchWithUrl(INPUT_TEXT_DATA_URL);

        clickNodeToShowInsertionHandle(INPUT_TEXT_ID);

        InsertionHandleController ihc = getContentViewCore().getInsertionHandleControllerForTest();
        HandleView handle = ihc.getHandleViewForTest();

        int initialX = handle.getPositionX();
        int initialY = handle.getPositionY();
        int dragToX = initialX;
        int dragToY = initialY + 150;

        // A vertical drag should not move the insertion handle.
        dragHandleTo(dragToX, dragToY);
        assertWaitForHandleNear(initialX, initialY);

        // The input box does not go to the edge of the screen, and neither should the insertion
        // handle.
        dragToX = getContentViewCore().getContainerView().getWidth();
        dragHandleTo(dragToX, dragToY);
        assertTrue(handle.getPositionX() < dragToX - 100);
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        // No way to just clear clipboard, so setting to empty string instead.
        saveToClipboard("");
    }

    private void clickNodeToShowInsertionHandle(String nodeId) throws Throwable {
        // On the first click the keyboard will be displayed but no insertion handles. On the second
        // click (only if it changes the selection), the insertion handle is displayed. So that the
        // second click changes the selection, the two clicks should be in sufficiently different
        // locations.
        Rect nodeBounds = DOMUtils.getNodeBounds(getContentViewCore(), nodeId);

        RenderCoordinates renderCoordinates = getContentViewCore().getRenderCoordinates();
        int offsetX = getContentViewCore().getViewportSizeOffsetWidthPix();
        int offsetY = getContentViewCore().getViewportSizeOffsetHeightPix();
        float left = renderCoordinates.fromLocalCssToPix(nodeBounds.left) + offsetX;
        float right = renderCoordinates.fromLocalCssToPix(nodeBounds.right) + offsetX;
        float top = renderCoordinates.fromLocalCssToPix(nodeBounds.top) + offsetY;
        float bottom = renderCoordinates.fromLocalCssToPix(nodeBounds.bottom) + offsetY;

        TouchCommon touchCommon = new TouchCommon(this);
        touchCommon.singleClickView(getContentViewCore().getContainerView(),
                (int)(left + 3 * (right - left) / 4), (int)(top + (bottom - top) / 2));


        TestTouchUtils.sleepForDoubleTapTimeout(getInstrumentation());
        assertTrue(waitForHasSelectionPosition());

        // TODO(cjhopman): Wait for keyboard display finished?
        touchCommon.singleClickView(getContentViewCore().getContainerView(),
                (int)(left + (right - left) / 4), (int)(top + (bottom - top) / 2));
        assertTrue(waitForHandleShowingEquals(true));
        assertTrue(waitForHandleViewStopped());
    }

    private boolean waitForHandleViewStopped() throws Throwable {
        // If the polling interval is too short, slowly moving may be detected as not moving.
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

    private void dragHandleTo(int dragToX, int dragToY, int steps) throws Throwable {
        InsertionHandleController ihc = getContentViewCore().getInsertionHandleControllerForTest();
        HandleView handle = ihc.getHandleViewForTest();
        int initialX = handle.getPositionX();
        int initialY = handle.getPositionY();
        View view = getContentViewCore().getContainerView();

        int fromLocation[] = TestTouchUtils.getAbsoluteLocationFromRelative(view, initialX,
                initialY + VERTICAL_OFFSET);
        int toLocation[] = TestTouchUtils.getAbsoluteLocationFromRelative(view, dragToX,
                dragToY + VERTICAL_OFFSET);

        long downTime = TestTouchUtils.dragStart(getInstrumentation(), fromLocation[0],
                fromLocation[1]);
        assertWaitForHandleDraggingEquals(true);
        TestTouchUtils.dragTo(getInstrumentation(), fromLocation[0], toLocation[0],
                fromLocation[1], toLocation[1], steps, downTime);
        TestTouchUtils.dragEnd(getInstrumentation(), toLocation[0], toLocation[1], downTime);
        assertWaitForHandleDraggingEquals(false);
        assertTrue(waitForHandleViewStopped());
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

    private static boolean isHandleNear(HandleView handle, int x, int y) {
        return (Math.abs(handle.getPositionX() - x) < HANDLE_POSITION_TOLERANCE) &&
                (Math.abs(handle.getPositionY() - VERTICAL_OFFSET - y) < HANDLE_POSITION_TOLERANCE);
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

    private void pasteOnMainSync() {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().getInsertionHandleControllerForTest().paste();
            }
        });
    }

    private void unselectOnMainSync() {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().getImeAdapterForTest().unselect();
            }
        });
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
}
