// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBarControlLayout.ControlLayoutParams;

/**
 * Tests for InfoBarControlLayout.  This suite doesn't check for specific details, like margins
 * paddings, and instead focuses on whether controls are placed correctly.
 */
public class InfoBarControlLayoutTest extends InstrumentationTestCase {
    private static final int SWITCH_ID_1 = 1;
    private static final int SWITCH_ID_2 = 2;
    private static final int SWITCH_ID_3 = 3;
    private static final int SWITCH_ID_4 = 4;
    private static final int SWITCH_ID_5 = 5;

    private int mMaxInfoBarWidth;
    private Context mContext;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getTargetContext();
        mContext.setTheme(R.style.MainTheme);
        mMaxInfoBarWidth = mContext.getResources().getDimensionPixelSize(R.dimen.infobar_max_width);
    }

    /**
     * Tests the layout algorithm on a set of five controls, the second of which is a huge control
     * and takes up the whole line.  The other smaller controls try to pack themselves as tightly
     * as possible, resulting in a layout like this:
     *
     * -----------------------
     * | A        | empty    |
     * -----------------------
     * | B                   |
     * -----------------------
     * | C        | D        |
     * -----------------------
     * | E        | empty    |
     * -----------------------
     */
    @SmallTest
    @UiThreadTest
    public void testComplexSwitchLayout() {
        // Add five controls to the layout, with the second being as wide as the layout.
        InfoBarControlLayout layout = new InfoBarControlLayout(mContext);
        layout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        View switch1 = layout.addSwitch(0, "A", SWITCH_ID_1, false);
        View switch2 = layout.addSwitch(0, "B", SWITCH_ID_2, false);
        View switch3 = layout.addSwitch(0, "C", SWITCH_ID_3, false);
        View switch4 = layout.addSwitch(0, "D", SWITCH_ID_4, false);
        View switch5 = layout.addSwitch(0, "E", SWITCH_ID_4, false);
        switch2.setMinimumWidth(mMaxInfoBarWidth);

        // Trigger the measurement algorithm.
        int parentWidthSpec =
                MeasureSpec.makeMeasureSpec(mMaxInfoBarWidth, MeasureSpec.AT_MOST);
        int parentHeightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        layout.measure(parentWidthSpec, parentHeightSpec);

        ControlLayoutParams params1 = InfoBarControlLayout.getControlLayoutParams(switch1);
        ControlLayoutParams params2 = InfoBarControlLayout.getControlLayoutParams(switch2);
        ControlLayoutParams params3 = InfoBarControlLayout.getControlLayoutParams(switch3);
        ControlLayoutParams params4 = InfoBarControlLayout.getControlLayoutParams(switch4);
        ControlLayoutParams params5 = InfoBarControlLayout.getControlLayoutParams(switch5);

        // Small control takes only half the width of the layout.
        assertEquals(0, params1.top);
        assertEquals(0, params1.start);
        assertTrue(switch1.getMeasuredWidth() < mMaxInfoBarWidth);

        // Big control gets shunted onto the next row and takes up the whole space.
        assertTrue(params2.top > switch1.getMeasuredHeight());
        assertEquals(0, params2.start);
        assertEquals(mMaxInfoBarWidth, switch2.getMeasuredWidth());

        // Small control gets placed onto the next line and takes only half the width.
        int bottomOfSwitch2 = params2.top + switch2.getMeasuredHeight();
        assertTrue(params3.top > bottomOfSwitch2);
        assertEquals(0, params3.start);
        assertTrue(switch3.getMeasuredWidth() < mMaxInfoBarWidth);

        // Small control gets placed next to the previous small control.
        assertEquals(params3.top, params4.top);
        assertTrue(params4.start > switch3.getMeasuredWidth());
        assertTrue(switch4.getMeasuredWidth() < mMaxInfoBarWidth);

        // Last small control has no room left and gets put on its own line.
        int bottomOfSwitch4 = params4.top + switch4.getMeasuredHeight();
        assertTrue(params5.top > bottomOfSwitch4);
        assertEquals(0, params5.start);
        assertTrue(switch5.getMeasuredWidth() < mMaxInfoBarWidth);
    }

    /**
     * Tests that the message is always the full width of the layout.
     */
    @SmallTest
    @UiThreadTest
    public void testFullWidthMessageControl() {
        // Add two controls to the layout.  The main message automatically requests the full width.
        InfoBarControlLayout layout = new InfoBarControlLayout(mContext);
        layout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        View view1 = layout.addMainMessage("A");
        View view2 = layout.addSwitch(0, "B", SWITCH_ID_2, false);

        // Trigger the measurement algorithm.
        int parentWidthSpec =
                MeasureSpec.makeMeasureSpec(mMaxInfoBarWidth, MeasureSpec.AT_MOST);
        int parentHeightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        layout.measure(parentWidthSpec, parentHeightSpec);

        ControlLayoutParams params1 = InfoBarControlLayout.getControlLayoutParams(view1);
        ControlLayoutParams params2 = InfoBarControlLayout.getControlLayoutParams(view2);

        // Main message takes up the full space.
        assertEquals(0, params1.top);
        assertEquals(0, params1.start);
        assertEquals(mMaxInfoBarWidth, view1.getMeasuredWidth());

        // Small control gets shunted onto the next row.
        assertTrue(params2.top > view1.getMeasuredHeight());
        assertEquals(0, params2.start);
        assertTrue(mMaxInfoBarWidth > view2.getMeasuredWidth());
    }
}
