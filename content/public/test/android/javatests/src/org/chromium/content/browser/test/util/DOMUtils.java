// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import android.graphics.Rect;
import android.test.ActivityInstrumentationTestCase2;
import android.util.JsonReader;

import junit.framework.Assert;

import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;

import java.io.IOException;
import java.io.StringReader;
import java.util.concurrent.TimeoutException;

/**
 * Collection of DOM-based utilities.
 */
public class DOMUtils {
    /**
     * Pauses the video with given {@code nodeId}.
     */
    public static void pauseVideo(final WebContents webContents, final String nodeId)
            throws InterruptedException, TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var video = document.getElementById('" + nodeId + "');");
        sb.append("  if (video) video.pause();");
        sb.append("})();");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents, sb.toString());
    }

    /**
     * Returns whether the video with given {@code nodeId} is paused.
     */
    public static boolean isVideoPaused(final WebContents webContents, final String nodeId)
            throws InterruptedException, TimeoutException {
        return getNodeField("paused", webContents, nodeId, Boolean.class);
    }

    /**
     * Waits until the playback of the video with given {@code nodeId} has started.
     *
     * @return Whether the playback has started.
     */
    public static boolean waitForVideoPlay(final WebContents webContents, final String nodeId)
            throws InterruptedException {
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return !DOMUtils.isVideoPaused(webContents, nodeId);
                } catch (InterruptedException e) {
                    // Intentionally do nothing
                    return false;
                } catch (TimeoutException e) {
                    // Intentionally do nothing
                    return false;
                }
            }
        });
    }

    /**
     * Returns whether the document is in fullscreen.
     */
    public static boolean isFullscreen(final WebContents webContents)
            throws InterruptedException, TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  return [document.webkitIsFullScreen];");
        sb.append("})();");

        String jsonText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents, sb.toString());
        return readValue(jsonText, Boolean.class);
    }

    /**
     * Makes the document exit fullscreen.
     */
    public static void exitFullscreen(final WebContents webContents) {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  if (document.webkitExitFullscreen) document.webkitExitFullscreen();");
        sb.append("})();");

        JavaScriptUtils.executeJavaScript(webContents, sb.toString());
    }

    /**
     * Returns the rect boundaries for a node by its id.
     */
    public static Rect getNodeBounds(final WebContents webContents, String nodeId)
            throws InterruptedException, TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = document.getElementById('" + nodeId + "');");
        sb.append("  if (!node) return null;");
        sb.append("  var width = Math.round(node.offsetWidth);");
        sb.append("  var height = Math.round(node.offsetHeight);");
        sb.append("  var x = -window.scrollX;");
        sb.append("  var y = -window.scrollY;");
        sb.append("  do {");
        sb.append("    x += node.offsetLeft;");
        sb.append("    y += node.offsetTop;");
        sb.append("  } while (node = node.offsetParent);");
        sb.append("  return [ Math.round(x), Math.round(y), width, height ];");
        sb.append("})();");

        String jsonText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents, sb.toString());

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
     * Focus a DOM node by its id.
     */
    public static void focusNode(final WebContents webContents, String nodeId)
            throws InterruptedException, TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = document.getElementById('" + nodeId + "');");
        sb.append("  if (node) node.focus();");
        sb.append("})();");

        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, sb.toString());
    }

    /**
     * Click a DOM node by its id.
     */
    public static void clickNode(ActivityInstrumentationTestCase2 activityTestCase,
            final ContentViewCore viewCore, String nodeId)
            throws InterruptedException, TimeoutException {
        int[] clickTarget = getClickTargetForNode(viewCore, nodeId);
        TouchCommon.singleClickView(viewCore.getContainerView(), clickTarget[0], clickTarget[1]);
    }

    /**
     * Long-press a DOM node by its id.
     */
    public static void longPressNode(ActivityInstrumentationTestCase2 activityTestCase,
            final ContentViewCore viewCore, String nodeId)
            throws InterruptedException, TimeoutException {
        int[] clickTarget = getClickTargetForNode(viewCore, nodeId);
        TouchCommon.longPressView(viewCore.getContainerView(), clickTarget[0], clickTarget[1]);
    }

    /**
     * Scrolls the view to ensure that the required DOM node is visible.
     */
    public static void scrollNodeIntoView(WebContents webContents, String nodeId)
            throws InterruptedException, TimeoutException {
        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                "document.getElementById('" + nodeId + "').scrollIntoView()");
    }

    /**
     * Returns the contents of the node by its id.
     */
    public static String getNodeContents(WebContents webContents, String nodeId)
            throws InterruptedException, TimeoutException {
        return getNodeField("textContent", webContents, nodeId, String.class);
    }

    /**
     * Returns the value of the node by its id.
     */
    public static String getNodeValue(final WebContents webContents, String nodeId)
            throws InterruptedException, TimeoutException {
        return getNodeField("value", webContents, nodeId, String.class);
    }

    /**
     * Returns the string value of a field of the node by its id.
     */
    public static String getNodeField(String fieldName, final WebContents webContents,
            String nodeId)
            throws InterruptedException, TimeoutException {
        return getNodeField(fieldName, webContents, nodeId, String.class);
    }

    private static <T> T getNodeField(String fieldName, final WebContents webContents,
            String nodeId, Class<T> valueType)
            throws InterruptedException, TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = document.getElementById('" + nodeId + "');");
        sb.append("  if (!node) return null;");
        sb.append("  return [ node." + fieldName + " ];");
        sb.append("})();");

        String jsonText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents, sb.toString());
        Assert.assertFalse("Failed to retrieve contents for " + nodeId,
                jsonText.trim().equalsIgnoreCase("null"));
        return readValue(jsonText, valueType);
    }

    private static <T> T readValue(String jsonText, Class<T> valueType) {
        JsonReader jsonReader = new JsonReader(new StringReader(jsonText));
        T value = null;
        try {
            jsonReader.beginArray();
            if (jsonReader.hasNext()) value = readValue(jsonReader, valueType);
            jsonReader.endArray();
            Assert.assertNotNull("Invalid contents returned.", value);

            jsonReader.close();
        } catch (IOException exception) {
            Assert.fail("Failed to evaluate JavaScript: " + jsonText + "\n" + exception);
        }
        return value;
    }

    @SuppressWarnings("unchecked")
    private static <T> T readValue(JsonReader jsonReader, Class<T> valueType)
            throws IOException {
        if (valueType.equals(String.class)) return ((T) jsonReader.nextString());
        if (valueType.equals(Boolean.class)) return ((T) ((Boolean) jsonReader.nextBoolean()));
        if (valueType.equals(Integer.class)) return ((T) ((Integer) jsonReader.nextInt()));
        if (valueType.equals(Long.class)) return ((T) ((Long) jsonReader.nextLong()));
        if (valueType.equals(Double.class)) return ((T) ((Double) jsonReader.nextDouble()));

        throw new IllegalArgumentException("Cannot read values of type " + valueType);
    }

    /**
     * Wait until a given node has non-zero bounds.
     * @return Whether the node started having non-zero bounds.
     */
    public static boolean waitForNonZeroNodeBounds(final WebContents webContents,
            final String nodeName)
            throws InterruptedException {
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return !DOMUtils.getNodeBounds(webContents, nodeName).isEmpty();
                } catch (InterruptedException e) {
                    // Intentionally do nothing
                    return false;
                } catch (TimeoutException e) {
                    // Intentionally do nothing
                    return false;
                }
            }
        });
    }

    /**
     * Returns click targets for a given DOM node.
     */
    private static int[] getClickTargetForNode(ContentViewCore viewCore, String nodeName)
            throws InterruptedException, TimeoutException {
        Rect bounds = getNodeBounds(viewCore.getWebContents(), nodeName);
        Assert.assertNotNull("Failed to get DOM element bounds of '" + nodeName + "'.", bounds);

        int topControlsLayoutHeight = viewCore.doTopControlsShrinkBlinkSize()
                ? viewCore.getTopControlsHeightPix() : 0;
        int clickX = (int) viewCore.getRenderCoordinates().fromLocalCssToPix(bounds.exactCenterX());
        int clickY = (int) viewCore.getRenderCoordinates().fromLocalCssToPix(bounds.exactCenterY())
                + topControlsLayoutHeight;
        return new int[] { clickX, clickY };
    }
}
