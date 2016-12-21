// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.annotation.TargetApi;
import android.graphics.Matrix;
import android.graphics.RectF;
import android.os.Build;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;
import android.text.TextUtils;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content.browser.RenderCoordinates;
import org.chromium.content.browser.test.util.TestInputMethodManagerWrapper;

/**
 * Test for {@link CursorAnchorInfoController}.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class CursorAnchorInfoControllerTest extends InstrumentationTestCase {
    private static RenderCoordinates createRenderCoordinates(float deviceScaleFactor,
            float contentOffsetYPix) {
        RenderCoordinates renderCoordinates = new RenderCoordinates();
        renderCoordinates.setFrameInfoForTest(deviceScaleFactor, contentOffsetYPix);
        return renderCoordinates;
    }

    private static final class TestViewDelegate implements CursorAnchorInfoController.ViewDelegate {
        public int locationX;
        public int locationY;
        @Override
        public void getLocationOnScreen(View view, int[] location) {
            location[0] = locationX;
            location[1] = locationY;
        }
    }

    private static final class TestComposingTextDelegate
            implements CursorAnchorInfoController.ComposingTextDelegate {
        private String mText;
        private int mSelectionStart = -1;
        private int mSelectionEnd = -1;
        private int mComposingTextStart = -1;
        private int mComposingTextEnd = -1;

        @Override
        public CharSequence getText() {
            return mText;
        }
        @Override
        public int getSelectionStart() {
            return mSelectionStart;
        }
        @Override
        public int getSelectionEnd() {
            return mSelectionEnd;
        }
        @Override
        public int getComposingTextStart() {
            return mComposingTextStart;
        }
        @Override
        public int getComposingTextEnd() {
            return mComposingTextEnd;
        }

        public void updateTextAndSelection(CursorAnchorInfoController controller,
                String text, int compositionStart, int compositionEnd, int selectionStart,
                int selectionEnd) {
            mText = text;
            mSelectionStart = selectionStart;
            mSelectionEnd = selectionEnd;
            mComposingTextStart = compositionStart;
            mComposingTextEnd = compositionEnd;
            controller.invalidateLastCursorAnchorInfo();
        }

        public void clearTextAndSelection(CursorAnchorInfoController controller) {
            updateTextAndSelection(controller, null, -1, -1, -1, -1);
        }
    }

    private void assertScaleAndTranslate(float expectedScale, float expectedTranslateX,
            float expectedTranslateY, CursorAnchorInfo actual) {
        Matrix expectedMatrix = new Matrix();
        expectedMatrix.setScale(expectedScale, expectedScale);
        expectedMatrix.postTranslate(expectedTranslateX, expectedTranslateY);
        assertEquals(expectedMatrix, actual.getMatrix());
    }

    private void assertHasInsertionMarker(int expectedFlags, float expectedHorizontal,
            float expectedTop, float expectedBaseline, float expectedBottom,
            CursorAnchorInfo actual) {
        assertEquals(expectedFlags, actual.getInsertionMarkerFlags());
        assertEquals(expectedHorizontal, actual.getInsertionMarkerHorizontal());
        assertEquals(expectedTop, actual.getInsertionMarkerTop());
        assertEquals(expectedBaseline, actual.getInsertionMarkerBaseline());
        assertEquals(expectedBottom, actual.getInsertionMarkerBottom());
    }

    private void assertHasNoInsertionMarker(CursorAnchorInfo actual) {
        assertEquals(0, actual.getInsertionMarkerFlags());
        assertTrue(Float.isNaN(actual.getInsertionMarkerHorizontal()));
        assertTrue(Float.isNaN(actual.getInsertionMarkerTop()));
        assertTrue(Float.isNaN(actual.getInsertionMarkerBaseline()));
        assertTrue(Float.isNaN(actual.getInsertionMarkerBottom()));
    }

    private void assertComposingText(CharSequence expectedComposingText,
            int expectedComposingTextStart, CursorAnchorInfo actual) {
        assertTrue(TextUtils.equals(expectedComposingText, actual.getComposingText()));
        assertEquals(expectedComposingTextStart, actual.getComposingTextStart());
    }

    private void assertSelection(int expecteSelectionStart, int expecteSelectionEnd,
            CursorAnchorInfo actual) {
        assertEquals(expecteSelectionStart, actual.getSelectionStart());
        assertEquals(expecteSelectionEnd, actual.getSelectionEnd());
    }

    @SmallTest
    @Feature({"Input-Text-IME"})
    public void testFocusedNodeChanged() {
        TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(null);
        TestViewDelegate viewDelegate = new TestViewDelegate();
        TestComposingTextDelegate composingTextDelegate = new TestComposingTextDelegate();
        CursorAnchorInfoController controller = CursorAnchorInfoController.createForTest(
                immw, composingTextDelegate, viewDelegate);
        View view = null;

        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;

        assertFalse(
                "IC#onRequestCursorUpdates() must be rejected if the focused node is not editable.",
                controller.onRequestCursorUpdates(
                        false /* immediate request */, true /* monitor request */, view));

        // Make sure that the focused node is considered to be non-editable by default.
        controller.setCompositionCharacterBounds(new float[] {0.0f, 1.0f, 2.0f, 3.0f}, view);
        composingTextDelegate.updateTextAndSelection(controller, "0", 0, 1, 0, 1);
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(0, immw.getUpdateCursorAnchorInfoCounter());

        controller.focusedNodeChanged(false);
        composingTextDelegate.clearTextAndSelection(controller);

        // Make sure that the controller does not crash even if it is called while the focused node
        // is not editable.
        controller.setCompositionCharacterBounds(new float[] {30.0f, 1.0f, 32.0f, 3.0f}, view);
        composingTextDelegate.updateTextAndSelection(controller, "1", 0, 1, 0, 1);
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 100.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(0, immw.getUpdateCursorAnchorInfoCounter());
    }

    @SmallTest
    @Feature({"Input-Text-IME"})
    public void testImmediateMode() {
        TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(null);
        TestViewDelegate viewDelegate = new TestViewDelegate();
        TestComposingTextDelegate composingTextDelegate = new TestComposingTextDelegate();
        CursorAnchorInfoController controller = CursorAnchorInfoController.createForTest(
                immw, composingTextDelegate, viewDelegate);
        View view = null;
        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;

        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);

        // Make sure that #updateCursorAnchorInfo() is not be called until the matrix info becomes
        // available with #onUpdateFrameInfo().
        assertTrue(controller.onRequestCursorUpdates(
                true /* immediate request */, false /* monitor request */, view));
        controller.setCompositionCharacterBounds(new float[] {0.0f, 1.0f, 2.0f, 3.0f}, view);
        composingTextDelegate.updateTextAndSelection(controller, "0", 0, 1, 0, 1);
        assertEquals(0, immw.getUpdateCursorAnchorInfoCounter());
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        assertHasInsertionMarker(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION, 2.0f, 0.0f, 3.0f,
                3.0f, immw.getLastCursorAnchorInfo());
        assertEquals(new RectF(0.0f, 1.0f, 2.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        assertEquals(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        assertComposingText("0", 0, immw.getLastCursorAnchorInfo());
        assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that 2nd call of #onUpdateFrameInfo() is ignored.
        controller.onUpdateFrameInfo(createRenderCoordinates(2.0f, 0.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());

        // Make sure that #onUpdateFrameInfo() is immediately called because the matrix info is
        // already available.
        assertTrue(controller.onRequestCursorUpdates(
                true /* immediate request */, false /* monitor request */, view));
        assertEquals(2, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(2.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        assertHasInsertionMarker(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION, 2.0f, 0.0f, 3.0f,
                3.0f, immw.getLastCursorAnchorInfo());
        assertEquals(new RectF(0.0f, 1.0f, 2.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        assertEquals(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        assertComposingText("0", 0, immw.getLastCursorAnchorInfo());
        assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that CURSOR_UPDATE_IMMEDIATE and CURSOR_UPDATE_MONITOR can be specified at
        // the same time.
        assertTrue(controller.onRequestCursorUpdates(
                true /* immediate request*/, true /* monitor request */, view));
        assertEquals(3, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(2.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(4, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        assertHasInsertionMarker(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION, 2.0f, 0.0f, 3.0f,
                3.0f, immw.getLastCursorAnchorInfo());
        assertEquals(new RectF(0.0f, 1.0f, 2.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        assertEquals(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        assertComposingText("0", 0, immw.getLastCursorAnchorInfo());
        assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that CURSOR_UPDATE_IMMEDIATE is cleared if the focused node becomes
        // non-editable.
        controller.focusedNodeChanged(false);
        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        assertTrue(controller.onRequestCursorUpdates(
                true /* immediate request */, false /* monitor request */, view));
        controller.focusedNodeChanged(false);
        composingTextDelegate.clearTextAndSelection(controller);
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 100.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(4, immw.getUpdateCursorAnchorInfoCounter());

        // Make sure that CURSOR_UPDATE_IMMEDIATE can be enabled again.
        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        assertTrue(controller.onRequestCursorUpdates(
                true /* immediate request */, false /* monitor request */, view));
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(5, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        assertHasInsertionMarker(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION, 2.0f, 0.0f, 3.0f,
                3.0f, immw.getLastCursorAnchorInfo());
        assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        assertComposingText(null, -1, immw.getLastCursorAnchorInfo());
        assertSelection(-1, -1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();
    }

    @SmallTest
    @Feature({"Input-Text-IME"})
    public void testMonitorMode() {
        TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(null);
        TestViewDelegate viewDelegate = new TestViewDelegate();
        TestComposingTextDelegate composingTextDelegate = new TestComposingTextDelegate();
        CursorAnchorInfoController controller = CursorAnchorInfoController.createForTest(
                immw, composingTextDelegate, viewDelegate);
        View view = null;
        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;

        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);

        // Make sure that #updateCursorAnchorInfo() is not be called until the matrix info becomes
        // available with #onUpdateFrameInfo().
        assertTrue(controller.onRequestCursorUpdates(
                false /* immediate request */, true /* monitor request */, view));
        controller.setCompositionCharacterBounds(new float[] {0.0f, 1.0f, 2.0f, 3.0f}, view);
        composingTextDelegate.updateTextAndSelection(controller, "0", 0, 1, 0, 1);
        assertEquals(0, immw.getUpdateCursorAnchorInfoCounter());
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        assertHasInsertionMarker(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION, 2.0f, 0.0f, 3.0f,
                3.0f, immw.getLastCursorAnchorInfo());
        assertEquals(new RectF(0.0f, 1.0f, 2.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        assertEquals(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        assertComposingText("0", 0, immw.getLastCursorAnchorInfo());
        assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that #updateCursorAnchorInfo() is not be called if any coordinate parameter is
        // changed for better performance.
        controller.setCompositionCharacterBounds(new float[] {0.0f, 1.0f, 2.0f, 3.0f}, view);
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());

        // Make sure that #updateCursorAnchorInfo() is called if #setCompositionCharacterBounds()
        // is called with a different parameter.
        controller.setCompositionCharacterBounds(new float[] {30.0f, 1.0f, 32.0f, 3.0f}, view);
        assertEquals(2, immw.getUpdateCursorAnchorInfoCounter());
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(2, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        assertHasInsertionMarker(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION, 2.0f, 0.0f, 3.0f,
                3.0f, immw.getLastCursorAnchorInfo());
        assertEquals(new RectF(30.0f, 1.0f, 32.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        assertEquals(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        assertComposingText("0", 0, immw.getLastCursorAnchorInfo());
        assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that #updateCursorAnchorInfo() is called if #updateTextAndSelection()
        // is called with a different parameter.
        composingTextDelegate.updateTextAndSelection(controller, "1", 0, 1, 0, 1);
        assertEquals(2, immw.getUpdateCursorAnchorInfoCounter());
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(3, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        assertHasInsertionMarker(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION, 2.0f, 0.0f, 3.0f,
                3.0f, immw.getLastCursorAnchorInfo());
        assertEquals(new RectF(30.0f, 1.0f, 32.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        assertEquals(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        assertComposingText("1", 0, immw.getLastCursorAnchorInfo());
        assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that #updateCursorAnchorInfo() is called if #onUpdateFrameInfo()
        // is called with a different parameter.
        controller.onUpdateFrameInfo(createRenderCoordinates(2.0f, 0.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(4, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(2.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        assertHasInsertionMarker(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION, 2.0f, 0.0f, 3.0f,
                3.0f, immw.getLastCursorAnchorInfo());
        assertEquals(new RectF(30.0f, 1.0f, 32.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        assertEquals(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        assertComposingText("1", 0, immw.getLastCursorAnchorInfo());
        assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that #updateCursorAnchorInfo() is called when the view origin is changed.
        viewDelegate.locationX = 7;
        viewDelegate.locationY = 9;
        controller.onUpdateFrameInfo(createRenderCoordinates(2.0f, 0.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(5, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(2.0f, 7.0f, 9.0f, immw.getLastCursorAnchorInfo());
        assertHasInsertionMarker(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION, 2.0f, 0.0f, 3.0f,
                3.0f, immw.getLastCursorAnchorInfo());
        assertEquals(new RectF(30.0f, 1.0f, 32.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        assertEquals(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        assertComposingText("1", 0, immw.getLastCursorAnchorInfo());
        assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that CURSOR_UPDATE_IMMEDIATE is cleared if the focused node becomes
        // non-editable.
        controller.focusedNodeChanged(false);
        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        assertTrue(controller.onRequestCursorUpdates(
                false /* immediate request */, true /* monitor request */, view));
        controller.focusedNodeChanged(false);
        composingTextDelegate.clearTextAndSelection(controller);
        controller.setCompositionCharacterBounds(new float[] {0.0f, 1.0f, 2.0f, 3.0f}, view);
        composingTextDelegate.updateTextAndSelection(controller, "0", 0, 1, 0, 1);
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(5, immw.getUpdateCursorAnchorInfoCounter());

        // Make sure that CURSOR_UPDATE_MONITOR can be enabled again.
        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        assertTrue(controller.onRequestCursorUpdates(
                false /* immediate request */, true /* monitor request */, view));
        controller.setCompositionCharacterBounds(new float[] {0.0f, 1.0f, 2.0f, 3.0f}, view);
        composingTextDelegate.updateTextAndSelection(controller, "0", 0, 1, 0, 1);
        assertEquals(5, immw.getUpdateCursorAnchorInfoCounter());
        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                true, true, 2.0f, 0.0f, 3.0f, view);
        assertEquals(6, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        assertHasInsertionMarker(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION, 2.0f, 0.0f, 3.0f,
                3.0f, immw.getLastCursorAnchorInfo());
        assertEquals(new RectF(0.0f, 1.0f, 2.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        assertEquals(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        assertComposingText("0", 0, immw.getLastCursorAnchorInfo());
        assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();
    }

    @SmallTest
    @Feature({"Input-Text-IME"})
    public void testSetCompositionCharacterBounds() {
        TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(null);
        TestViewDelegate viewDelegate = new TestViewDelegate();
        TestComposingTextDelegate composingTextDelegate = new TestComposingTextDelegate();
        CursorAnchorInfoController controller = CursorAnchorInfoController.createForTest(
                immw, composingTextDelegate, viewDelegate);
        View view = null;

        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;

        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        assertTrue(controller.onRequestCursorUpdates(
                false /* immediate request */, true /* monitor request */, view));

        composingTextDelegate.updateTextAndSelection(controller, "01234", 1, 3, 1, 1);
        controller.setCompositionCharacterBounds(new float[] {0.0f, 1.0f, 2.0f, 3.0f,
                4.0f, 1.1f, 6.0f, 2.9f}, view);
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                false, false, Float.NaN, Float.NaN, Float.NaN, view);
        assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());
        assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        assertEquals(new RectF(0.0f, 1.0f, 2.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(1));
        assertEquals(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(1));
        assertEquals(new RectF(4.0f, 1.1f, 6.0f, 2.9f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(2));
        assertEquals(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(2));
        assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(3));
        assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(3));
        assertComposingText("12", 1, immw.getLastCursorAnchorInfo());
        assertSelection(1, 1, immw.getLastCursorAnchorInfo());
    }

    @SmallTest
    @Feature({"Input-Text-IME"})
    public void testUpdateTextAndSelection() {
        TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(null);
        TestViewDelegate viewDelegate = new TestViewDelegate();
        TestComposingTextDelegate composingTextDelegate = new TestComposingTextDelegate();
        CursorAnchorInfoController controller = CursorAnchorInfoController.createForTest(
                immw, composingTextDelegate, viewDelegate);
        View view = null;

        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;

        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        assertTrue(controller.onRequestCursorUpdates(
                false /* immediate request */, true /* monitor request */, view));

        composingTextDelegate.updateTextAndSelection(controller, "01234", 3, 3, 1, 1);
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                false, false, Float.NaN, Float.NaN, Float.NaN, view);
        assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());
        assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(1));
        assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(1));
        assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(2));
        assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(2));
        assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(3));
        assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(3));
        assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(4));
        assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(4));
        assertComposingText("", 3, immw.getLastCursorAnchorInfo());
        assertSelection(1, 1, immw.getLastCursorAnchorInfo());
    }

    @SmallTest
    @Feature({"Input-Text-IME"})
    public void testInsertionMarker() {
        TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(null);
        TestViewDelegate viewDelegate = new TestViewDelegate();
        TestComposingTextDelegate composingTextDelegate = new TestComposingTextDelegate();
        CursorAnchorInfoController controller = CursorAnchorInfoController.createForTest(
                immw, composingTextDelegate, viewDelegate);
        View view = null;

        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        assertTrue(controller.onRequestCursorUpdates(
                false /* immediate request */, true /* monitor request */, view));

        // Test no insertion marker.
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                false, false, Float.NaN, Float.NaN, Float.NaN, view);
        assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());
        assertHasNoInsertionMarker(immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Test a visible insertion marker.
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                true, true, 10.0f, 23.0f, 29.0f, view);
        assertEquals(2, immw.getUpdateCursorAnchorInfoCounter());
        assertHasInsertionMarker(CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                10.0f, 23.0f, 29.0f, 29.0f, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Test a invisible insertion marker.
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                true, false, 10.0f, 23.0f, 29.0f, view);
        assertEquals(3, immw.getUpdateCursorAnchorInfoCounter());
        assertHasInsertionMarker(CursorAnchorInfo.FLAG_HAS_INVISIBLE_REGION,
                10.0f, 23.0f, 29.0f, 29.0f, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();
    }

    @SmallTest
    @Feature({"Input-Text-IME"})
    public void testMatrix() {
        TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(null);
        TestViewDelegate viewDelegate = new TestViewDelegate();
        TestComposingTextDelegate composingTextDelegate = new TestComposingTextDelegate();
        CursorAnchorInfoController controller = CursorAnchorInfoController.createForTest(
                immw, composingTextDelegate, viewDelegate);
        View view = null;

        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        assertTrue(controller.onRequestCursorUpdates(
                false /* immediate request */, true /* monitor request */, view));

        // Test no transformation
        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;
        controller.onUpdateFrameInfo(createRenderCoordinates(1.0f, 0.0f),
                false, false, Float.NaN, Float.NaN, Float.NaN, view);
        assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // device scale factor == 2.0
        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;
        controller.onUpdateFrameInfo(createRenderCoordinates(2.0f, 0.0f),
                false, false, Float.NaN, Float.NaN, Float.NaN, view);
        assertEquals(2, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(2.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // device scale factor == 2.0
        // view origin == (10, 141)
        viewDelegate.locationX = 10;
        viewDelegate.locationY = 141;
        controller.onUpdateFrameInfo(createRenderCoordinates(2.0f, 0.0f),
                false, false, Float.NaN, Float.NaN, Float.NaN, view);
        assertEquals(3, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(2.0f, 10.0f, 141.0f, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // device scale factor == 2.0
        // content offset Y = 40.0f
        // view origin == (10, 141)
        viewDelegate.locationX = 10;
        viewDelegate.locationY = 141;
        controller.onUpdateFrameInfo(createRenderCoordinates(2.0f, 40.0f),
                false, false, Float.NaN, Float.NaN, Float.NaN, view);
        assertEquals(4, immw.getUpdateCursorAnchorInfoCounter());
        assertScaleAndTranslate(2.0f, 10.0f, 181.0f, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();
    }
}
