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
        runLayoutTest(ALIGN_START, false, false);
        runLayoutTest(ALIGN_START, false, true);
        runLayoutTest(ALIGN_START, true, false);
        runLayoutTest(ALIGN_START, true, true);

        runLayoutTest(ALIGN_APART, false, false);
        runLayoutTest(ALIGN_APART, false, true);
        runLayoutTest(ALIGN_APART, true, false);
        runLayoutTest(ALIGN_APART, true, true);

        runLayoutTest(ALIGN_END, false, false);
        runLayoutTest(ALIGN_END, false, true);
        runLayoutTest(ALIGN_END, true, false);
        runLayoutTest(ALIGN_END, true, true);
    }

    /** Lays out two controls that fit on the same line. */
    private void runLayoutTest(int alignment, boolean isRtl, boolean addSecondView) {
        DualControlLayout layout = new DualControlLayout(mContext, null);
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
            assertEquals("Primary should be on the right.", INFOBAR_WIDTH, primary.getRight());
        } else {
            assertEquals("Primary should be on the left.", 0, primary.getLeft());
        }
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
                    assertEquals(INFOBAR_WIDTH, secondary.getRight());
                } else {
                    // Secondary View is on the far left.
                    assertTrue(secondary.getRight() < primary.getLeft());
                    assertEquals(0, secondary.getLeft());
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

        assertEquals(layout.getMeasuredHeight(), primary.getMeasuredHeight());
    }

    @SmallTest
    @UiThreadTest
    public void testStacked() {
        runStackedLayoutTest(ALIGN_START, false);
        runStackedLayoutTest(ALIGN_START, true);
        runStackedLayoutTest(ALIGN_APART, false);
        runStackedLayoutTest(ALIGN_APART, true);
        runStackedLayoutTest(ALIGN_END, false);
        runStackedLayoutTest(ALIGN_END, true);
    }

    /** Runs a test where the controls don't fit on the same line. */
    private void runStackedLayoutTest(int alignment, boolean isRtl) {
        DualControlLayout layout = new DualControlLayout(mContext, null);
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

        assertEquals(0, primary.getLeft());
        assertEquals(0, secondary.getLeft());

        assertEquals(INFOBAR_WIDTH, primary.getRight());
        assertEquals(INFOBAR_WIDTH, secondary.getRight());

        assertEquals(INFOBAR_WIDTH, primary.getMeasuredWidth());
        assertEquals(INFOBAR_WIDTH, secondary.getMeasuredWidth());
        assertEquals(INFOBAR_WIDTH, layout.getMeasuredWidth());

        assertEquals(primary.getBottom() + STACKED_MARGIN, secondary.getTop());
        MoreAsserts.assertNotEqual(layout.getMeasuredHeight(), primary.getMeasuredHeight());
    }
}
