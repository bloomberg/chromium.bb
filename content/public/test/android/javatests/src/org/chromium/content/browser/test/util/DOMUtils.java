// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.graphics.Rect;
import android.util.JsonReader;

import org.junit.Assert;

import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;

import java.io.IOException;
import java.io.StringReader;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Collection of DOM-based utilities.
 */
public class DOMUtils {

    private static final long MEDIA_TIMEOUT_SECONDS = scaleTimeout(10);
    private static final long MEDIA_TIMEOUT_MILLISECONDS = MEDIA_TIMEOUT_SECONDS * 1000;

    /**
     * Plays the media with given {@code id}.
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to be played.
     */
    public static void playMedia(final WebContents webContents, final String id)
            throws InterruptedException, TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var media = document.getElementById('" + id + "');");
        sb.append("  if (media) media.play();");
        sb.append("})();");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents, sb.toString(),
                MEDIA_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    /**
     * Pauses the media with given {@code id}
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to be paused.
     */
    public static void pauseMedia(final WebContents webContents, final String id)
            throws InterruptedException, TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var media = document.getElementById('" + id + "');");
        sb.append("  if (media) media.pause();");
        sb.append("})();");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents, sb.toString(),
                MEDIA_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    /**
     * Returns whether the media with given {@code id} is paused.
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to check.
     * @return whether the media is paused.
     */
    public static boolean isMediaPaused(final WebContents webContents, final String id)
            throws InterruptedException, TimeoutException {
        return getNodeField("paused", webContents, id, Boolean.class);
    }

    /**
     * Returns whether the media with given {@code id} has ended.
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to check.
     * @return whether the media has ended.
     */
    private static boolean isMediaEnded(final WebContents webContents, final String id)
            throws InterruptedException, TimeoutException {
        return getNodeField("ended", webContents, id, Boolean.class);
    }

    /**
     * Returns the current time of the media with given {@code id}.
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to check.
     * @return the current time (in seconds) of the media.
     */
    private static double getCurrentTime(final WebContents webContents, final String id)
            throws InterruptedException, TimeoutException {
        return getNodeField("currentTime", webContents, id, Double.class);
    }

    /**
     * Waits until the playback of the media with given {@code id} has started.
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to check.
     */
    public static void waitForMediaPlay(final WebContents webContents, final String id) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    // Playback can't be reliably detected until current time moves forward.
                    return !DOMUtils.isMediaPaused(webContents, id)
                            && DOMUtils.getCurrentTime(webContents, id) > 0;
                } catch (InterruptedException e) {
                    // Intentionally do nothing
                    return false;
                } catch (TimeoutException e) {
                    // Intentionally do nothing
                    return false;
                }
            }
        }, MEDIA_TIMEOUT_MILLISECONDS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Waits until the playback of the media with given {@code id} has paused before ended.
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to check.
     */
    public static void waitForMediaPauseBeforeEnd(final WebContents webContents, final String id) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return DOMUtils.isMediaPaused(webContents, id)
                            && !DOMUtils.isMediaEnded(webContents, id);
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
     * Returns whether the document is fullscreen.
     * @param webContents The WebContents to check.
     * @return Whether the document is fullsscreen.
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
     * @param webContents The WebContents to make fullscreen.
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
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @return The rect boundaries for the node.
     */
    public static Rect getNodeBounds(final WebContents webContents, String nodeId)
            throws InterruptedException, TimeoutException {
        String jsCode = "document.getElementById('" + nodeId + "')";
        return getNodeBoundsByJs(webContents, jsCode);
    }

    /**
     * Focus a DOM node by its id.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
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
     * Get the id of the currently focused node.
     * @param webContents The WebContents in which the node lives.
     * @return The id of the currently focused node.
     */
    public static String getFocusedNode(WebContents webContents)
            throws InterruptedException, TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = document.activeElement;");
        sb.append("  if (!node) return null;");
        sb.append("  return node.id;");
        sb.append("})();");

        String id = JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, sb.toString());

        // String results from JavaScript includes surrounding quotes.  Remove them.
        if (id != null && id.length() >= 2 && id.charAt(0) == '"') {
            id = id.substring(1, id.length() - 1);
        }
        return id;
    }

    /**
     * Click a DOM node by its id, scrolling it into view first.
     * @param viewCore The ContentViewCore in which the node lives.
     * @param nodeId The id of the node.
     */
    public static boolean clickNode(final ContentViewCore viewCore, String nodeId)
            throws InterruptedException, TimeoutException {
        scrollNodeIntoView(viewCore.getWebContents(), nodeId);
        int[] clickTarget = getClickTargetForNode(viewCore, nodeId);
        return TouchCommon.singleClickView(
                viewCore.getContainerView(), clickTarget[0], clickTarget[1]);
    }

    /**
     * Click a DOM node returned by JS code, scrolling it into view first.
     * @param viewCore The ContentViewCore in which the node lives.
     * @param jsCode The JS code to find the node.
     */
    public static void clickNodeByJs(final ContentViewCore viewCore, String jsCode)
            throws InterruptedException, TimeoutException {
        scrollNodeIntoViewByJs(viewCore.getWebContents(), jsCode);
        int[] clickTarget = getClickTargetForNodeByJs(viewCore, jsCode);
        TouchCommon.singleClickView(viewCore.getContainerView(), clickTarget[0], clickTarget[1]);
    }

    /**
     * Click a given rect in the page. Does not move the rect into view.
     * @param viewCore The ContentViewCore in which the node lives.
     * @param rect The rect to click.
     */
    public static boolean clickRect(final ContentViewCore viewCore, Rect rect)
            throws InterruptedException, TimeoutException {
        int[] clickTarget = getClickTargetForBounds(viewCore, rect);
        return TouchCommon.singleClickView(
                viewCore.getContainerView(), clickTarget[0], clickTarget[1]);
    }

    /**
     * Long-press a DOM node by its id, scrolling it into view first.
     * @param viewCore The ContentViewCore in which the node lives.
     * @param nodeId The id of the node.
     */
    public static void longPressNode(final ContentViewCore viewCore, String nodeId)
            throws InterruptedException, TimeoutException {
        scrollNodeIntoView(viewCore.getWebContents(), nodeId);
        String jsCode = "document.getElementById('" + nodeId + "')";
        longPressNodeByJs(viewCore, jsCode);
    }

    /**
     * Long-press a DOM node by its id.
     * <p>Note that content view should be located in the current position for a foreseeable
     * amount of time because this involves sleep to simulate touch to long press transition.
     * @param viewCore The ContentViewCore in which the node lives.
     * @param nodeId The id of the node.
     */
    public static void longPressNodeByJs(final ContentViewCore viewCore, String jsCode)
            throws InterruptedException, TimeoutException {
        int[] clickTarget = getClickTargetForNodeByJs(viewCore, jsCode);
        TouchCommon.longPressView(viewCore.getContainerView(), clickTarget[0], clickTarget[1]);
    }

    /**
     * Scrolls the view to ensure that the required DOM node is visible.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     */
    public static void scrollNodeIntoView(WebContents webContents, String nodeId)
            throws InterruptedException, TimeoutException {
        scrollNodeIntoViewByJs(webContents, "document.getElementById('" + nodeId + "')");
    }

    /**
     * Scrolls the view to ensure that the required DOM node is visible.
     * @param webContents The WebContents in which the node lives.
     * @param jsCode The JS code to find the node.
     */
    public static void scrollNodeIntoViewByJs(WebContents webContents, String jsCode)
            throws InterruptedException, TimeoutException {
        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                jsCode + ".scrollIntoView()");
    }

    /**
     * Returns the text contents of a given node.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @return the text contents of the node.
     */
    public static String getNodeContents(WebContents webContents, String nodeId)
            throws InterruptedException, TimeoutException {
        return getNodeField("textContent", webContents, nodeId, String.class);
    }

    /**
     * Returns the value of a given node.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @return the value of the node.
     */
    public static String getNodeValue(final WebContents webContents, String nodeId)
            throws InterruptedException, TimeoutException {
        return getNodeField("value", webContents, nodeId, String.class);
    }

    /**
     * Returns the string value of a field of a given node.
     * @param fieldName The field to return the value from.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @return the value of the field.
     */
    public static String getNodeField(String fieldName, final WebContents webContents,
            String nodeId)
            throws InterruptedException, TimeoutException {
        return getNodeField(fieldName, webContents, nodeId, String.class);
    }

    /**
     * Wait until a given node has non-zero bounds.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     */
    public static void waitForNonZeroNodeBounds(final WebContents webContents,
            final String nodeId) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return !DOMUtils.getNodeBounds(webContents, nodeId).isEmpty();
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
     * Returns the value of a given field of type {@code valueType} as a {@code T}.
     * @param fieldName The field to return the value from.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @param valueType The type of the value to read.
     * @return the field's value.
     */
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

    /**
     * Returns the next value of type {@code valueType} as a {@code T}.
     * @param jsonText The unparsed json text.
     * @param valueType The type of the value to read.
     * @return the read value.
     */
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

    /**
     * Returns the next value of type {@code valueType} as a {@code T}.
     * @param jsonReader JsonReader instance to be used.
     * @param valueType The type of the value to read.
     * @throws IllegalArgumentException If the {@code valueType} isn't known.
     * @return the read value.
     */
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
     * Returns click target for a given DOM node.
     * @param viewCore The ContentViewCore in which the node lives.
     * @param nodeId The id of the node.
     * @return the click target of the node in the form of a [ x, y ] array.
     */
    private static int[] getClickTargetForNode(ContentViewCore viewCore, String nodeId)
            throws InterruptedException, TimeoutException {
        String jsCode = "document.getElementById('" + nodeId + "')";
        return getClickTargetForNodeByJs(viewCore, jsCode);
    }

    /**
     * Returns click target for a given DOM node.
     * @param viewCore The ContentViewCore in which the node lives.
     * @param jsCode The javascript to get the node.
     * @return the click target of the node in the form of a [ x, y ] array.
     */
    private static int[] getClickTargetForNodeByJs(ContentViewCore viewCore, String jsCode)
            throws InterruptedException, TimeoutException {
        Rect bounds = getNodeBoundsByJs(viewCore.getWebContents(), jsCode);
        Assert.assertNotNull(
                "Failed to get DOM element bounds of element='" + jsCode + "'.", bounds);

        return getClickTargetForBounds(viewCore, bounds);
    }

    /**
     * Returns click target for the DOM node specified by the rect boundaries.
     * @param viewCore The ContentViewCore in which the node lives.
     * @param bounds The rect boundaries of a DOM node.
     * @return the click target of the node in the form of a [ x, y ] array.
     */
    private static int[] getClickTargetForBounds(ContentViewCore viewCore, Rect bounds) {
        int browserControlsLayoutHeight = viewCore.doBrowserControlsShrinkBlinkSize()
                ? viewCore.getTopControlsHeightPix()
                : 0;
        int clickX = (int) viewCore.getRenderCoordinates().fromLocalCssToPix(bounds.exactCenterX());
        int clickY = (int) viewCore.getRenderCoordinates().fromLocalCssToPix(bounds.exactCenterY())
                + browserControlsLayoutHeight;
        return new int[] { clickX, clickY };
    }

    /**
     * Returns the rect boundaries for a node by the javascript to get the node.
     * @param webContents The WebContents in which the node lives.
     * @param jsCode The javascript to get the node.
     * @return The rect boundaries for the node.
     */
    private static Rect getNodeBoundsByJs(final WebContents webContents, String jsCode)
            throws InterruptedException, TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = " + jsCode + ";");
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

        Assert.assertFalse("Failed to retrieve bounds for element: " + jsCode,
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
}
