// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH;
import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX;
import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY;

import android.graphics.RectF;
import android.os.Build;
import android.os.Bundle;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.lang.reflect.Method;

/**
 * Tests for WebContentsAccessibility.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebContentsAccessibilityTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    /**
     * Helper class that can be used to wait until an AccessibilityEvent is fired on a view.
     */
    private static class AccessibilityEventCallbackHelper extends CallbackHelper {
        AccessibilityEventCallbackHelper(View view) {
            view.setAccessibilityDelegate(new View.AccessibilityDelegate() {
                @Override
                public boolean onRequestSendAccessibilityEvent(
                        ViewGroup host, View child, AccessibilityEvent event) {
                    AccessibilityEventCallbackHelper.this.notifyCalled();
                    return true;
                }
            });
        }
    };

    AccessibilityEventCallbackHelper mAccessibilityEventCallbackHelper;

    /**
     * Test Android O API to retrieve character bounds from an accessible node.
     */
    @Test
    @MediumTest
    public void testAddExtraDataToAccessibilityNodeInfo() throws Throwable {
        // The API we want to test only exists on O and higher.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;

        // Load a really simple webpage.
        final String data = "<h1>Simple test page</h1>"
                + "<section><p>Text</p></section>";
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

        // Get the AccessibilityNodeProvider.
        ContentViewCore contentViewCore = mActivityTestRule.getContentViewCore();
        contentViewCore.setAccessibilityState(true);
        AccessibilityNodeProvider provider = contentViewCore.getAccessibilityNodeProvider();

        // Wait until we find a node in the accessibility tree with the text "Text".
        // Whenever the tree is updated, an AccessibilityEvent is fired, so we can just wait until
        // the next event before checking again.
        mAccessibilityEventCallbackHelper =
                new AccessibilityEventCallbackHelper(contentViewCore.getContainerView());
        int textNodeVirtualViewId = View.NO_ID;
        do {
            mAccessibilityEventCallbackHelper.waitForCallback(
                    mAccessibilityEventCallbackHelper.getCallCount());

            textNodeVirtualViewId = findNodeWithText(provider, View.NO_ID, "Text");
        } while (textNodeVirtualViewId == View.NO_ID);

        // Now call the API we want to test - addExtraDataToAccessibilityNodeInfo.
        AccessibilityNodeInfo textNode =
                provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotEquals(textNode, null);
        Bundle arguments = new Bundle();
        arguments.putInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX, 0);
        arguments.putInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH, 4);
        provider.addExtraDataToAccessibilityNodeInfo(
                textNodeVirtualViewId, textNode, EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY, arguments);

        // It should return a result, but all of the rects will be the same because it hasn't
        // loaded inline text boxes yet.
        Bundle extras = textNode.getExtras();
        RectF[] result =
                (RectF[]) extras.getParcelableArray(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);
        Assert.assertNotEquals(result, null);
        Assert.assertEquals(result.length, 4);
        Assert.assertEquals(result[0], result[1]);
        Assert.assertEquals(result[0], result[2]);
        Assert.assertEquals(result[0], result[3]);

        // The role string should be a camel cased programmatic identifier.
        CharSequence roleString = extras.getCharSequence("AccessibilityNodeInfo.chromeRole");
        Assert.assertEquals("staticText", roleString.toString());

        // Wait for inline text boxes to load. Unfortunately we don't have a signal for this,
        // we have to keep sleeping until it loads. In practice it only takes a few milliseconds.
        do {
            Thread.sleep(10);

            textNode = provider.createAccessibilityNodeInfo(textNodeVirtualViewId);
            provider.addExtraDataToAccessibilityNodeInfo(textNodeVirtualViewId, textNode,
                    EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY, arguments);
            extras = textNode.getExtras();
            result = (RectF[]) extras.getParcelableArray(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);
            Assert.assertEquals(result.length, 4);
        } while (result[0].equals(result[1]));

        // The final result should be the separate bounding box of all four characters.
        Assert.assertNotEquals(result[0], result[1]);
        Assert.assertNotEquals(result[0], result[2]);
        Assert.assertNotEquals(result[0], result[3]);

        // All four should have nonzero left, top, width, and height
        for (int i = 0; i < 4; ++i) {
            Assert.assertTrue(result[i].left > 0);
            Assert.assertTrue(result[i].top > 0);
            Assert.assertTrue(result[i].width() > 0);
            Assert.assertTrue(result[i].height() > 0);
        }

        // They should be in order.
        Assert.assertTrue(result[0].left < result[1].left);
        Assert.assertTrue(result[1].left < result[2].left);
        Assert.assertTrue(result[2].left < result[3].left);
    }

    /**
     * Helper method to call AccessibilityNodeInfo.getChildId and convert to a virtual
     * view ID using reflection, since the needed methods are hidden.
     */
    private int getChildId(AccessibilityNodeInfo node, int index) {
        try {
            Method getChildIdMethod =
                    AccessibilityNodeInfo.class.getMethod("getChildId", int.class);
            long childId = (long) getChildIdMethod.invoke(node, Integer.valueOf(index));
            Method getVirtualDescendantIdMethod =
                    AccessibilityNodeInfo.class.getMethod("getVirtualDescendantId", long.class);
            int virtualViewId =
                    (int) getVirtualDescendantIdMethod.invoke(null, Long.valueOf(childId));
            return virtualViewId;
        } catch (Exception ex) {
            Assert.fail("Unable to call hidden AccessibilityNodeInfo method: " + ex.toString());
            return 0;
        }
    }

    /**
     * Helper method to recursively search a tree of virtual views under an
     * AccessibilityNodeProvider and return one whose text or contentDescription equals |text|.
     * Returns the virtual view ID of the matching node, if found, and View.NO_ID if not.
     */
    private int findNodeWithText(
            AccessibilityNodeProvider provider, int virtualViewId, String text) {
        AccessibilityNodeInfo node = provider.createAccessibilityNodeInfo(virtualViewId);
        Assert.assertNotEquals(node, null);

        if (text.equals(node.getText()) || text.equals(node.getContentDescription())) {
            return virtualViewId;
        }

        for (int i = 0; i < node.getChildCount(); i++) {
            int childId = getChildId(node, i);
            AccessibilityNodeInfo child = provider.createAccessibilityNodeInfo(childId);
            if (child != null) {
                int result = findNodeWithText(provider, childId, text);
                if (result != View.NO_ID) return result;
            }
        }

        return View.NO_ID;
    }
}
