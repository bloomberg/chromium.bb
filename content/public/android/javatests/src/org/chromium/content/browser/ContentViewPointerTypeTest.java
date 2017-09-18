// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.graphics.Rect;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.blink_public.web.WebCursorInfoType;
import org.chromium.content.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_shell.ShellViewAndroidDelegate.OnCursorUpdateHelper;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

/**
 * Tests that we can move mouse cursor and test cursor icon.
 */
@RunWith(ContentJUnit4ClassRunner.class)
public class ContentViewPointerTypeTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final String CURSOR_PAGE = UrlUtils.encodeHtmlDataUri("<html><body>"
            + "<style> div {height:33%; width:100%;} </style>"
            + "<div id=\"hand\" style=\"cursor:pointer;\"></div>"
            + "<div id=\"text\" style=\"cursor:text;\"></div>"
            + "<div id=\"help\" style=\"cursor:help;\"></div>"
            + "</body></html>");

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchContentShellWithUrl(CURSOR_PAGE);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
    }

    private void moveCursor(final float x, final float y) {
        MotionEvent.PointerProperties[] pointerProperties = new MotionEvent.PointerProperties[1];
        MotionEvent.PointerProperties pp = new MotionEvent.PointerProperties();
        pp.id = 0;
        pp.toolType = MotionEvent.TOOL_TYPE_MOUSE;
        pointerProperties[0] = pp;

        MotionEvent.PointerCoords[] pointerCoords = new MotionEvent.PointerCoords[1];
        MotionEvent.PointerCoords pc = new MotionEvent.PointerCoords();
        pc.x = x;
        pc.y = y;
        pointerCoords[0] = pc;

        MotionEvent cursorMoveEvent = MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis() + 1, MotionEvent.ACTION_HOVER_MOVE, 1, pointerProperties,
                pointerCoords, 0, 0, 1.0f, 1.0f, 0, 0, InputDevice.SOURCE_MOUSE, 0);
        cursorMoveEvent.setSource(InputDevice.SOURCE_MOUSE);
        mActivityTestRule.getContentViewCore().getContainerView().dispatchGenericMotionEvent(
                cursorMoveEvent);
    }

    private void checkPointerTypeForNode(final String nodeId, final int type) throws Throwable {
        final Rect rect = DOMUtils.getNodeBounds(mActivityTestRule.getWebContents(), nodeId);
        OnCursorUpdateHelper onCursorUpdateHelper = mActivityTestRule.getOnCursorUpdateHelper();
        int onCursorUpdateCount = onCursorUpdateHelper.getCallCount();
        mActivityTestRule.runOnUiThreadForTestCommon(new Runnable() {
            @Override
            public void run() {
                float x = mActivityTestRule.getContentViewCore()
                                  .getRenderCoordinates()
                                  .fromLocalCssToPix((float) (rect.left + rect.right) / 2.0f);
                float y = mActivityTestRule.getContentViewCore()
                                  .getRenderCoordinates()
                                  .fromLocalCssToPix((float) (rect.top + rect.bottom) / 2.0f);
                moveCursor(x, y);
            }
        });
        onCursorUpdateHelper.waitForCallback(onCursorUpdateCount);
        Assert.assertEquals(type, onCursorUpdateHelper.getPointerType());
    }

    @Test
    //@SmallTest
    //@Feature({"Main"})
    @DisabledTest(message = "crbug.com/755112")
    public void testPointerType() throws Throwable {
        checkPointerTypeForNode("hand", WebCursorInfoType.TYPE_HAND);
        checkPointerTypeForNode("text", WebCursorInfoType.TYPE_I_BEAM);
        checkPointerTypeForNode("help", WebCursorInfoType.TYPE_HELP);
    }
}
