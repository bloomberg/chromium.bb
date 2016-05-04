// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import static org.chromium.chrome.browser.widget.DualControlLayout.ALIGN_APART;
import static org.chromium.chrome.browser.widget.DualControlLayout.ALIGN_END;
import static org.chromium.chrome.browser.widget.DualControlLayout.ALIGN_START;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.MoreAsserts;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;
import android.widget.Space;

import org.chromium.chrome.R;

/**
 * Tests for DualControlLayout.
 */
public class DualControlLayoutTest extends InstrumentationTestCase {
    private static final int PRIMARY_HEIGHT = 16;
    private static final int SECONDARY_HEIGHT = 8;
    private static final int STACKED_MARGIN = 4;
    private static final int INFOBAR_WIDTH = 3200;

    private static final int PADDING_LEFT = 8;
    private static final int PADDING_TOP = 16;
    private static final int PADDING_RIGHT = 32;
    private static final int PADDING_BOTTOM = 64;

    private int mTinyControlWidth;
    private Context mContext;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getTargetContext();
        mContext.setTheme(R.style.MainTheme);
        mTinyControlWidth = INFOBAR_WIDTH / 4;
    }

    @SmallTest
    @UiThreadTest
    public void testAlignSideBySide() {
        runLayoutTest(ALIGN_START, false, false, false);
        runLayoutTest(ALIGN_START, false, true, false);
        runLayoutTest(ALIGN_START, true, false, false);
        runLayoutTest(ALIGN_START, true, true, false);

        runLayoutTest(ALIGN_APART, false, false, false);
        runLayoutTest(ALIGN_APART, false, true, false);
        runLayoutTest(ALIGN_APART, true, false, false);
        runLayoutTest(ALIGN_APART, true, true, false);

        runLayoutTest(ALIGN_END, false, false, false);
        runLayoutTest(ALIGN_END, false, true, false);
        runLayoutTest(ALIGN_END, true, false, false);
        runLayoutTest(ALIGN_END, true, true, false);

        // Test the padding.
        runLayoutTest(ALIGN_START, false, false, true);
        runLayoutTest(ALIGN_START, false, true, true);
        runLayoutTest(ALIGN_START, true, false, true);
        runLayoutTest(ALIGN_START, true, true, true);

        runLayoutTest(ALIGN_APART, false, false, true);
        runLayoutTest(ALIGN_APART, false, true, true);
        runLayoutTest(ALIGN_APART, true, false, true);
        runLayoutTest(ALIGN_APART, true, true, true);

        runLayoutTest(ALIGN_END, false, false, true);
        runLayoutTest(ALIGN_END, false, true, true);
        runLayoutTest(ALIGN_END, true, false, true);
        runLayoutTest(ALIGN_END, true, true, true);
    }

    /** Lays out two controls that fit on the same line. */
    private void runLayoutTest(
            int alignment, boolean isRtl, boolean addSecondView, boolean addPadding) {
        DualControlLayout layout = new DualControlLayout(mContext, null);
        if (addPadding) layout.setPadding(PADDING_LEFT, PADDING_TOP, PADDING_RIGHT, PADDING_BOTTOM);
        layout.setAlignment(alignment);
        layout.setLayoutDirection(isRtl ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);
        layout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        View primary = new Space(mContext);
        primary.setMinimumWidth(mTinyControlWidth);
        primary.setMinimumHeight(PRIMARY_HEIGHT);
        layout.addView(primary);

        View secondary = null;
        if (addSecondView) {
            secondary = new Space(mContext);
            secondary.setMinimumWidth(mTinyControlWidth);
            secondary.setMinimumHeight(SECONDARY_HEIGHT);
            layout.addView(secondary);
        }

        // Trigger the measurement & layout algorithms.
        int parentWidthSpec = MeasureSpec.makeMeasureSpec(INFOBAR_WIDTH, MeasureSpec.EXACTLY);
        int parentHeightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        layout.measure(parentWidthSpec, parentHeightSpec);
        layout.layout(0, 0, layout.getMeasuredWidth(), layout.getMeasuredHeight());

        // Confirm that the primary View is in the correct place.
        if ((isRtl && alignment == ALIGN_START)
                || (!isRtl && (alignment == ALIGN_APART || alignment == ALIGN_END))) {
            int expectedRight = INFOBAR_WIDTH - (addPadding ? PADDING_RIGHT : 0);
            assertEquals("Primary should be on the right.", expectedRight, primary.getRight());
        } else {
            int expectedLeft = addPadding ? PADDING_LEFT : 0;
            assertEquals("Primary should be on the left.", expectedLeft, primary.getLeft());
        }
        int expectedTop = addPadding ? PADDING_TOP : 0;
        int expectedBottom = expectedTop + PRIMARY_HEIGHT;
        assertEquals("Primary top in wrong location", expectedTop, primary.getTop());
        assertEquals("Primary bottom in wrong location", expectedBottom, primary.getBottom());
        assertEquals(mTinyControlWidth, primary.getMeasuredWidth());
        assertEquals(PRIMARY_HEIGHT, primary.getMeasuredHeight());
        MoreAsserts.assertNotEqual(primary.getLeft(), primary.getRight());

        // Confirm that the secondary View is in the correct place.
        if (secondary != null) {
            assertEquals(mTinyControlWidth, secondary.getMeasuredWidth());
            assertEquals(SECONDARY_HEIGHT, secondary.getMeasuredHeight());
            MoreAsserts.assertNotEqual(secondary.getLeft(), secondary.getRight());
            if (alignment == ALIGN_START) {
                if (isRtl) {
                    // Secondary View is immediately to the left of the parent.
                    assertTrue(secondary.getRight() < primary.getLeft());
                    MoreAsserts.assertNotEqual(0, secondary.getLeft());
                } else {
                    // Secondary View is immediately to the right of the parent.
                    assertTrue(primary.getRight() < secondary.getLeft());
                    MoreAsserts.assertNotEqual(INFOBAR_WIDTH, secondary.getRight());
                }
            } else if (alignment == ALIGN_APART) {
                if (isRtl) {
                    // Secondary View is on the far right.
                    assertTrue(primary.getRight() < secondary.getLeft());
                    int expectedRight = INFOBAR_WIDTH - (addPadding ? PADDING_RIGHT : 0);
                    assertEquals(expectedRight, secondary.getRight());
                } else {
                    // Secondary View is on the far left.
                    assertTrue(secondary.getRight() < primary.getLeft());
                    int expectedLeft = addPadding ? PADDING_LEFT : 0;
                    assertEquals(expectedLeft, secondary.getLeft());
                }
            } else {
                assertEquals(ALIGN_END, alignment);
                if (isRtl) {
                    // Secondary View is immediately to the right of the parent.
                    assertTrue(primary.getRight() < secondary.getLeft());
                    MoreAsserts.assertNotEqual(INFOBAR_WIDTH, secondary.getRight());
                } else {
                    // Secondary View is immediately to the left of the parent.
                    assertTrue(secondary.getRight() < primary.getLeft());
                    MoreAsserts.assertNotEqual(0, secondary.getLeft());
                }
            }

            // Confirm that the secondary View is centered with respect to the first.
            int primaryCenter = (primary.getTop() + primary.getBottom()) / 2;
            int secondaryCenter = (secondary.getTop() + secondary.getBottom()) / 2;
            assertEquals(primaryCenter, secondaryCenter);
        }

        int expectedLayoutHeight =
                primary.getMeasuredHeight() + (addPadding ? PADDING_TOP + PADDING_BOTTOM : 0);
        assertEquals(expectedLayoutHeight, layout.getMeasuredHeight());
    }

    @SmallTest
    @UiThreadTest
    public void testStacked() {
        runStackedLayoutTest(ALIGN_START, false, false);
        runStackedLayoutTest(ALIGN_START, true, false);
        runStackedLayoutTest(ALIGN_APART, false, false);
        runStackedLayoutTest(ALIGN_APART, true, false);
        runStackedLayoutTest(ALIGN_END, false, false);
        runStackedLayoutTest(ALIGN_END, true, false);

        // Test the padding.
        runStackedLayoutTest(ALIGN_START, false, true);
        runStackedLayoutTest(ALIGN_START, true, true);
        runStackedLayoutTest(ALIGN_APART, false, true);
        runStackedLayoutTest(ALIGN_APART, true, true);
        runStackedLayoutTest(ALIGN_END, false, true);
        runStackedLayoutTest(ALIGN_END, true, true);
    }

    /** Runs a test where the controls don't fit on the same line.
     * @param addPadding TODO(dfalcantara):*/
    private void runStackedLayoutTest(int alignment, boolean isRtl, boolean addPadding) {
        DualControlLayout layout = new DualControlLayout(mContext, null);
        if (addPadding) layout.setPadding(PADDING_LEFT, PADDING_TOP, PADDING_RIGHT, PADDING_BOTTOM);
        layout.setAlignment(alignment);
        layout.setStackedMargin(STACKED_MARGIN);
        layout.setLayoutDirection(isRtl ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);
        layout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        View primary = new Space(mContext);
        primary.setMinimumWidth(mTinyControlWidth);
        primary.setMinimumHeight(PRIMARY_HEIGHT);
        layout.addView(primary);

        View secondary = new Space(mContext);
        secondary.setMinimumWidth(INFOBAR_WIDTH);
        secondary.setMinimumHeight(SECONDARY_HEIGHT);
        layout.addView(secondary);

        // Trigger the measurement & layout algorithms.
        int parentWidthSpec =
                MeasureSpec.makeMeasureSpec(INFOBAR_WIDTH, MeasureSpec.AT_MOST);
        int parentHeightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        layout.measure(parentWidthSpec, parentHeightSpec);
        layout.layout(0, 0, layout.getMeasuredWidth(), layout.getMeasuredHeight());

        assertEquals(addPadding ? PADDING_LEFT : 0, primary.getLeft());
        assertEquals(addPadding ? PADDING_LEFT : 0, secondary.getLeft());

        assertEquals(INFOBAR_WIDTH - (addPadding ? PADDING_RIGHT : 0), primary.getRight());
        assertEquals(INFOBAR_WIDTH - (addPadding ? PADDING_RIGHT : 0), secondary.getRight());

        int expectedControlWidth = INFOBAR_WIDTH - (addPadding ? PADDING_LEFT + PADDING_RIGHT : 0);
        assertEquals(expectedControlWidth, primary.getMeasuredWidth());
        assertEquals(expectedControlWidth, secondary.getMeasuredWidth());
        assertEquals(INFOBAR_WIDTH, layout.getMeasuredWidth());

        int expectedPrimaryTop = addPadding ? PADDING_TOP : 0;
        int expectedPrimaryBottom = expectedPrimaryTop + primary.getHeight();
        int expectedSecondaryTop = expectedPrimaryBottom + STACKED_MARGIN;
        int expectedSecondaryBottom = expectedSecondaryTop + secondary.getHeight();
        int expectedLayoutHeight = expectedSecondaryBottom + (addPadding ? PADDING_BOTTOM : 0);
        assertEquals(expectedPrimaryTop, primary.getTop());
        assertEquals(expectedPrimaryBottom, primary.getBottom());
        assertEquals(expectedSecondaryTop, secondary.getTop());
        assertEquals(expectedSecondaryBottom, secondary.getBottom());
        assertEquals(expectedLayoutHeight, layout.getMeasuredHeight());
        MoreAsserts.assertNotEqual(layout.getMeasuredHeight(), primary.getMeasuredHeight());
    }
}
