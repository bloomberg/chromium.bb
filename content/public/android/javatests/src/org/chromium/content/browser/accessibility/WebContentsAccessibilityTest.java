// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH;
import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX;
import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.graphics.RectF;
import android.os.Build;
import android.os.Bundle;
import android.support.test.filters.MediumTest;
import android.text.InputType;
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
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.ContentViewCore;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.lang.reflect.Method;

/**
 * Tests for WebContentsAccessibility. Actually tests WebContentsAccessibilityImpl that
 * implements the interface.
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
                    if (event.getEventType()
                            == AccessibilityEvent
                                       .TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY) {
                        sIndex = event.getFromIndex();
                    }
                    return true;
                }
            });
        }
    };

    /*
     * Enable accessibility and wait until ContentViewCore.getAccessibilityNodeProvider()
     * returns something not null.
     */
    private AccessibilityNodeProvider enableAccessibilityAndWaitForNodeProvider() {
        final WebContentsAccessibilityImpl wcax =
                WebContentsAccessibilityImpl.fromWebContents(mActivityTestRule.getWebContents());
        wcax.setState(true);
        wcax.setAccessibilityEnabledForTesting();

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return wcax.getAccessibilityNodeProvider() != null;
            }
        });

        return wcax.getAccessibilityNodeProvider();
    }

    AccessibilityEventCallbackHelper mAccessibilityEventCallbackHelper;
    /* Used to store the cursor indexes from TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY
     * events. Used in { @link testNavigationWithingEditTextField }
     */
    static int sIndex;

    /**
     * Test Android O API to retrieve character bounds from an accessible node.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @TargetApi(Build.VERSION_CODES.O)
    public void testAddExtraDataToAccessibilityNodeInfo() throws Throwable {
        // Load a really simple webpage.
        final String data = "<h1>Simple test page</h1>"
                + "<section><p>Text</p></section>";
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

        // Get the AccessibilityNodeProvider.
        ContentViewCore contentViewCore = mActivityTestRule.getContentViewCore();
        WebContentsAccessibilityImpl wcax =
                WebContentsAccessibilityImpl.fromWebContents(mActivityTestRule.getWebContents());
        wcax.setState(true);
        wcax.setAccessibilityEnabledForTesting();
        AccessibilityNodeProvider provider = wcax.getAccessibilityNodeProvider();

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

    /**
     * Helper method to recursively search a tree of virtual views under an
     * AccessibilityNodeProvider and return one whose input type is
     * {@link android.text.InputType#TYPE_CLASS_TEXT}.
     * Returns the virtual view ID of the matching node, if found, and View.NO_ID if not.
     */
    @SuppressLint("NewApi")
    private int findNodeWithTextInputType(
            AccessibilityNodeProvider provider, int virtualViewId, int type) {
        AccessibilityNodeInfo node = provider.createAccessibilityNodeInfo(virtualViewId);
        Assert.assertNotEquals(node, null);

        if (node.getInputType() == type) {
            return virtualViewId;
        }

        for (int i = 0; i < node.getChildCount(); i++) {
            int childId = getChildId(node, i);
            AccessibilityNodeInfo child = provider.createAccessibilityNodeInfo(childId);
            if (child != null) {
                int result =
                        findNodeWithTextInputType(provider, childId, InputType.TYPE_CLASS_TEXT);
                if (result != View.NO_ID) return result;
            }
        }
        return View.NO_ID;
    }

    /**
     * Test Android O API to check if an edit field can be traversed with granularity while typing.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @TargetApi(Build.VERSION_CODES.O)
    public void testNavigationWithingEditTextField() throws Throwable {
        // Load a really simple webpage.
        final String data = "<form>\n"
                + "  First name:<br>\n"
                + "  <input id=\"fn\" type=\"text\" value=\"Text\"><br>\n"
                + "</form>";
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        AccessibilityNodeProvider provider = enableAccessibilityAndWaitForNodeProvider();

        // Find a node in the accessibility tree with input type TYPE_CLASS_TEXT.
        ContentViewCore contentViewCore = mActivityTestRule.getContentViewCore();
        int editFieldVirtualViewId =
                findNodeWithTextInputType(provider, View.NO_ID, InputType.TYPE_CLASS_TEXT);

        mAccessibilityEventCallbackHelper =
                new AccessibilityEventCallbackHelper(contentViewCore.getContainerView());

        AccessibilityNodeInfo editTextNode =
                provider.createAccessibilityNodeInfo(editFieldVirtualViewId);

        // Assert we have got the correct node.
        Assert.assertNotEquals(editTextNode, null);
        Assert.assertEquals(editTextNode.getInputType(), InputType.TYPE_CLASS_TEXT);
        Assert.assertEquals(editTextNode.getText().toString(), "Text");

        boolean result1 = provider.performAction(
                editFieldVirtualViewId, AccessibilityNodeInfo.ACTION_FOCUS, null);
        boolean result2 = provider.performAction(
                editFieldVirtualViewId, AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null);
        boolean result3 = provider.performAction(editFieldVirtualViewId,
                AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS, null);

        // Assert all actions are performed successfully.
        Assert.assertEquals(result1, true);
        Assert.assertEquals(result2, true);
        Assert.assertEquals(result3, true);

        while (!editTextNode.isFocused()) {
            Thread.sleep(1);
            editTextNode.recycle();
            editTextNode = provider.createAccessibilityNodeInfo(editFieldVirtualViewId);
        }

        Bundle args = new Bundle();
        // Set granularity to Character.
        args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
                AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER);
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);

        // Simulate swipe left.
        for (int i = 3; i >= 0; i--) {
            boolean result = provider.performAction(editFieldVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);
            // Assert if the index of the character traversed is correct.
            Assert.assertEquals(sIndex, i);
        }
        // Simulate swipe right.
        for (int i = 0; i <= 3; i++) {
            boolean result = provider.performAction(editFieldVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);
            // Assert if the index of the character traversed is correct.
            Assert.assertEquals(sIndex, i);
        }
    }
}
