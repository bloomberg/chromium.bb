// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import android.graphics.Rect;
import android.test.ActivityInstrumentationTestCase2;
import android.util.JsonReader;

import java.io.IOException;
import java.io.StringReader;
import java.util.concurrent.TimeoutException;

import junit.framework.Assert;

import org.chromium.content.browser.ContentView;

/**
 * Collection of DOM-based utilities.
 */
public class DOMUtils {

    /**
     * Returns the rect boundaries for a node by its id.
     */
    public static Rect getNodeBounds(
            final ContentView view, TestCallbackHelperContainer viewClient, String nodeId)
            throws InterruptedException, TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = document.getElementById('" + nodeId + "');");
        sb.append("  if (!node) return null;");
        sb.append("  var width = node.offsetWidth;");
        sb.append("  var height = node.offsetHeight;");
        sb.append("  var x = -window.scrollX;");
        sb.append("  var y = -window.scrollY;");
        sb.append("  do {");
        sb.append("    x += node.offsetLeft;");
        sb.append("    y += node.offsetTop;");
        sb.append("  } while (node = node.offsetParent);");
        sb.append("  return [ x, y, width, height ];");
        sb.append("})();");

        String jsonText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                view, viewClient, sb.toString());

        Assert.assertFalse("Failed to retrieve bounds for " + nodeId,
                jsonText.trim().equalsIgnoreCase("null"));

        JsonReader jsonReader = new JsonReader(new StringReader(jsonText));
        int[] bounds = new int[4];
        try {
            jsonReader.beginArray();
            int i = 0;
            while (jsonReader.hasNext()) {
                bounds[i++] = jsonReader.nextInt();
            }
            jsonReader.endArray();
            Assert.assertEquals("Invalid bounds returned.", 4, i);

            jsonReader.close();
        } catch (IOException exception) {
            Assert.fail("Failed to evaluate JavaScript: " + jsonText + "\n" + exception);
        }

        return new Rect(bounds[0], bounds[1], bounds[0] + bounds[2], bounds[1] + bounds[3]);
    }

    /**
     * Click a DOM node by its id.
     */
    public static void clickNode(ActivityInstrumentationTestCase2 activityTestCase,
            final ContentView view, TestCallbackHelperContainer viewClient, String nodeId)
            throws InterruptedException, TimeoutException {
        Rect bounds = getNodeBounds(view, viewClient, nodeId);
        Assert.assertNotNull("Failed to get DOM element bounds of '" + nodeId + "'.'", bounds);

        float scale = view.getScale();
        int clickX = (int)(bounds.exactCenterX() * scale + 0.5);
        int clickY = (int)(bounds.exactCenterY() * scale + 0.5);

        TouchCommon touchCommon = new TouchCommon(activityTestCase);
        touchCommon.singleClickView(view, clickX, clickY);
    }

    /**
     * Long-press a DOM node by its id.
     */
    public static void longPressNode(ActivityInstrumentationTestCase2 activityTestCase,
            final ContentView view, TestCallbackHelperContainer viewClient, String nodeId)
            throws InterruptedException, TimeoutException {
        Rect bounds = getNodeBounds(view, viewClient, nodeId);
        Assert.assertNotNull("Failed to get DOM element bounds of '" + nodeId + "'.'", bounds);

        float scale = view.getScale();
        int clickX = (int)(bounds.exactCenterX() * scale + 0.5);
        int clickY = (int)(bounds.exactCenterY() * scale + 0.5);

        TouchCommon touchCommon = new TouchCommon(activityTestCase);
        touchCommon.longPressView(view, clickX, clickY);
    }

    /**
     * Scrolls the view to ensure that the required DOM node is visible.
     */
    public static void scrollNodeIntoView(final ContentView view,
            TestCallbackHelperContainer viewClient, String nodeId)
            throws InterruptedException, TimeoutException {
        JavaScriptUtils.executeJavaScriptAndWaitForResult(view, viewClient,
                "document.getElementById('" + nodeId + "').scrollIntoView()");
    }

    /**
     * Returns the contents of the node by its id.
     */
    public static String getNodeContents(final ContentView view,
            TestCallbackHelperContainer viewClient, String nodeId)
            throws InterruptedException, TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = document.getElementById('" + nodeId + "');");
        sb.append("  if (!node) return null;");
        sb.append("  return [ node.textContent ];");
        sb.append("})();");

        String jsonText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                view, viewClient, sb.toString());
        Assert.assertFalse("Failed to retrieve contents for " + nodeId,
                jsonText.trim().equalsIgnoreCase("null"));

        JsonReader jsonReader = new JsonReader(new StringReader(jsonText));
        String contents = null;
        try {
            jsonReader.beginArray();
            if (jsonReader.hasNext()) contents = jsonReader.nextString();
            jsonReader.endArray();
            Assert.assertNotNull("Invalid contents returned.", contents);

            jsonReader.close();
        } catch (IOException exception) {
            Assert.fail("Failed to evaluate JavaScript: " + jsonText + "\n" + exception);
        }
        return contents;
    }
}
