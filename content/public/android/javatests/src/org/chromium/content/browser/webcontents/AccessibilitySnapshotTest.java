// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.AccessibilitySnapshotCallback;
import org.chromium.content_public.browser.AccessibilitySnapshotNode;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Accessibility snapshot tests for Assist feature.
 */
public class AccessibilitySnapshotTest extends ContentShellTestBase {
    private static class AccessibilityCallbackHelper extends CallbackHelper {
        private AccessibilitySnapshotNode mRoot;

        public void notifyCalled(AccessibilitySnapshotNode root) {
            mRoot = root;
            super.notifyCalled();
        }

        public AccessibilitySnapshotNode getValue() {
            return mRoot;
        }
    }

    private AccessibilitySnapshotNode receiveAccessibilitySnapshot(String data, String js)
            throws Throwable {
        launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(data));
        waitForActiveShellToBeDoneLoading();
        if (js != null) {
            JavaScriptUtils.executeJavaScriptAndWaitForResult(getWebContents(), js);
        }

        final AccessibilityCallbackHelper callbackHelper = new AccessibilityCallbackHelper();
        final AccessibilitySnapshotCallback callback = new AccessibilitySnapshotCallback() {
            @Override
            public void onAccessibilitySnapshot(AccessibilitySnapshotNode root) {
                callbackHelper.notifyCalled(root);
            }
        };
        // read the callbackcount before executing the call on UI thread, since it may
        // synchronously complete.
        final int callbackCount = callbackHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getWebContents().requestAccessibilitySnapshot(callback);
            }
        });
        callbackHelper.waitForCallback(callbackCount);
        return callbackHelper.getValue();
    }

    /**
     * Verifies that AX tree is returned.
     */
    @SmallTest
    public void testRequestAccessibilitySnapshot() throws Throwable {
        final String data = "<button>Click</button>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        assertEquals(1, root.children.size());
        assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        assertEquals(1, child.children.size());
        assertEquals("", child.text);
        AccessibilitySnapshotNode grandChild = child.children.get(0);
        assertEquals(0, grandChild.children.size());
        assertEquals("Click", grandChild.text);
    }

    @SmallTest
    public void testRequestAccessibilitySnapshotColors() throws Throwable {
        final String data = "<p style=\"color:#123456;background:#abcdef\">color</p>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        assertEquals(1, root.children.size());
        assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        assertEquals("color", child.text);
        assertTrue(child.hasStyle);
        assertEquals("ff123456", Integer.toHexString(child.color));
        assertEquals("ffabcdef", Integer.toHexString(child.bgcolor));
    }

    @SmallTest
    public void testRequestAccessibilitySnapshotFontSize() throws Throwable {
        final String data = "<html><head><style> "
                + "    p { font-size:16px; transform: scale(2); }"
                + "    </style></head><body><p>foo</p></body></html>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        assertEquals(1, root.children.size());
        assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        assertTrue(child.hasStyle);
        assertEquals("foo", child.text);

        // The font size should take the scale into account.
        assertEquals(32.0, child.textSize, 1.0);
    }

    @SmallTest
    public void testRequestAccessibilitySnapshotStyles() throws Throwable {
        final String data = "<html><head><style> "
                + "    body { font: italic bold 12px Courier; }"
                + "    </style></head><body><p>foo</p></body></html>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        assertEquals(1, root.children.size());
        assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        assertEquals("foo", child.text);
        assertTrue(child.hasStyle);
        assertTrue(child.bold);
        assertTrue(child.italic);
        assertFalse(child.lineThrough);
        assertFalse(child.underline);
    }

    @SmallTest
    public void testRequestAccessibilitySnapshotStrongStyle() throws Throwable {
        final String data = "<html><body><p>foo</p><p><strong>bar</strong></p></body></html>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        assertEquals(2, root.children.size());
        assertEquals("", root.text);
        AccessibilitySnapshotNode child1 = root.children.get(0);
        assertEquals("foo", child1.text);
        assertTrue(child1.hasStyle);
        assertFalse(child1.bold);
        AccessibilitySnapshotNode child2 = root.children.get(1);
        AccessibilitySnapshotNode child2child = child2.children.get(0);
        assertEquals("bar", child2child.text);
        assertEquals(child1.textSize, child2child.textSize);
        assertTrue(child2child.bold);
    }

    @SmallTest
    public void testRequestAccessibilitySnapshotItalicStyle() throws Throwable {
        final String data = "<html><body><i>foo</i></body></html>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        assertEquals(1, root.children.size());
        assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        assertEquals("foo", grandchild.text);
        assertTrue(grandchild.hasStyle);
        assertTrue(grandchild.italic);
    }

    @SmallTest
    public void testRequestAccessibilitySnapshotBoldStyle() throws Throwable {
        final String data = "<html><body><b>foo</b></body></html>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        assertEquals(1, root.children.size());
        assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        assertEquals("foo", grandchild.text);
        assertTrue(grandchild.hasStyle);
        assertTrue(grandchild.bold);
    }

    @SmallTest
    public void testRequestAccessibilitySnapshotNoStyle() throws Throwable {
        final String data = "<table><thead></thead></table>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        assertEquals(1, root.children.size());
        assertEquals("", root.text);
        AccessibilitySnapshotNode grandChild = root.children.get(0).children.get(0);
        assertFalse(grandChild.hasStyle);
    }

    private String getSelectionScript(String node1, int start, String node2, int end) {
        return "var element1 = document.getElementById('" + node1 + "');"
                + "var node1 = element1.childNodes.item(0);"
                + "var range=document.createRange();"
                + "range.setStart(node1," + start + ");"
                + "var element2 = document.getElementById('" + node2 + "');"
                + "var node2 = element2.childNodes.item(0);"
                + "range.setEnd(node2," + end + ");"
                + "var selection=window.getSelection();"
                + "selection.removeAllRanges();"
                + "selection.addRange(range);";
    }

    @SmallTest
    public void testRequestAccessibilitySnapshotOneCharacterSelection() throws Throwable {
        final String data = "<html><body><b id='node'>foo</b></body></html>";

        AccessibilitySnapshotNode root =
                receiveAccessibilitySnapshot(data, getSelectionScript("node", 0, "node", 1));
        assertEquals(1, root.children.size());
        assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        assertEquals("foo", grandchild.text);
        assertEquals(0, grandchild.startSelection);
        assertEquals(1, grandchild.endSelection);
    }

    @SmallTest
    public void testRequestAccessibilitySnapshotOneNodeSelection() throws Throwable {
        final String data = "<html><body><b id='node'>foo</b></body></html>";

        AccessibilitySnapshotNode root =
                receiveAccessibilitySnapshot(data, getSelectionScript("node", 0, "node", 3));
        assertEquals(1, root.children.size());
        assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        assertEquals("foo", grandchild.text);
        assertEquals(0, grandchild.startSelection);
        assertEquals(3, grandchild.endSelection);
    }

    @SmallTest
    public void testRequestAccessibilitySnapshotSubsequentNodeSelection() throws Throwable {
        final String data = "<html><body><b id='node1'>foo</b><b id='node2'>bar</b></body></html>";

        AccessibilitySnapshotNode root =
                receiveAccessibilitySnapshot(data, getSelectionScript("node1", 1, "node2", 1));
        assertEquals(1, root.children.size());
        assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        assertEquals("foo", grandchild.text);
        assertEquals(1, grandchild.startSelection);
        assertEquals(3, grandchild.endSelection);
        grandchild = child.children.get(1);
        assertEquals("bar", grandchild.text);
        assertEquals(0, grandchild.startSelection);
        assertEquals(1, grandchild.endSelection);
    }

    @SmallTest
    public void testRequestAccessibilitySnapshotMultiNodeSelection() throws Throwable {
        final String data =
                "<html><body><b id='node1'>foo</b><b>middle</b><b id='node2'>bar</b></body></html>";

        AccessibilitySnapshotNode root =
                receiveAccessibilitySnapshot(data, getSelectionScript("node1", 1, "node2", 1));
        assertEquals(1, root.children.size());
        assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        assertEquals("foo", grandchild.text);
        assertEquals(1, grandchild.startSelection);
        assertEquals(3, grandchild.endSelection);
        grandchild = child.children.get(1);
        assertEquals("middle", grandchild.text);
        assertEquals(0, grandchild.startSelection);
        assertEquals(6, grandchild.endSelection);
        grandchild = child.children.get(2);
        assertEquals("bar", grandchild.text);
        assertEquals(0, grandchild.startSelection);
        assertEquals(1, grandchild.endSelection);
    }

    @SmallTest
    public void testRequestAccessibilitySnapshotInputSelection() throws Throwable {
        final String data = "<html><body><input id='input' value='Hello, world'></body></html>";
        final String js = "var input = document.getElementById('input');"
                + "input.select();"
                + "input.selectionStart = 0;"
                + "input.selectionEnd = 5;";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, js);
        assertEquals(1, root.children.size());
        assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        assertEquals("Hello, world", grandchild.text);
        assertEquals(0, grandchild.startSelection);
        assertEquals(5, grandchild.endSelection);
    }

    @SmallTest
    public void testRequestAccessibilitySnapshotPasswordField() throws Throwable {
        final String data =
                "<html><body><input id='input' type='password' value='foo'></body></html>";
        AccessibilitySnapshotNode root = receiveAccessibilitySnapshot(data, null);
        assertEquals(1, root.children.size());
        assertEquals("", root.text);
        AccessibilitySnapshotNode child = root.children.get(0);
        AccessibilitySnapshotNode grandchild = child.children.get(0);
        assertEquals("•••", grandchild.text);
    }
}
