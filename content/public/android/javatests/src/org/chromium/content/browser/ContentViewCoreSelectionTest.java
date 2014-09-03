// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Integration tests for text selection-related behavior.
 */
public class ContentViewCoreSelectionTest extends ContentShellTestBase {
    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\"" +
            "content=\"width=device-width, initial-scale=1.1, maximum-scale=1.5\" /></head>" +
            "<body><form action=\"about:blank\">" +
            "<input id=\"empty_input_text\" type=\"text\" />" +
            "<br/><p><span id=\"plain_text_1\">This is Plain Text One</span></p>" +
            "<br/><p><span id=\"plain_text_2\">This is Plain Text Two</span></p>" +
            "<br/><input id=\"empty_input_text\" type=\"text\" />" +
            "<br/><input id=\"input_text\" type=\"text\" value=\"Sample Text\" />" +
            "<br/><textarea id=\"empty_textarea\" rows=\"2\" cols=\"20\"></textarea>" +
            "<br/><textarea id=\"textarea\" rows=\"2\" cols=\"20\">Sample Text</textarea>" +
            "</form></body></html>");

    private ContentViewCore mContentViewCore;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        launchContentShellWithUrl(DATA_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());

        mContentViewCore = getContentViewCore();
        assertWaitForPageScaleFactorMatch(1.1f);
        assertWaitForSelectActionBarStatus(false);
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectActionBarShownOnLongPressingPlainText() throws Exception {
       DOMUtils.longPressNode(this, mContentViewCore, "plain_text_1");
       assertWaitForSelectActionBarStatus(true);
       DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
       assertWaitForSelectActionBarStatus(false);
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectActionBarClearedOnTappingOutsideSelection() throws Exception {
       DOMUtils.longPressNode(this, mContentViewCore, "plain_text_1");
       assertWaitForSelectActionBarStatus(true);
       DOMUtils.clickNode(this, mContentViewCore, "plain_text_2");
       assertWaitForSelectActionBarStatus(false);
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectActionBarStaysOnLongPressingDifferentPlainTexts() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text_1");
        assertWaitForSelectActionBarStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text_2");
        assertWaitForSelectActionBarStatus(true);
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectActionBarStaysOnLongPressingDifferentNonEmptyInputs() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(true);
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectActionBarNotShownOnLongPressingDifferentEmptyInputs() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text_1");
        assertWaitForSelectActionBarStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForSelectActionBarStatus(false);
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text_2");
        assertWaitForSelectActionBarStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "empty_textarea");
        assertWaitForSelectActionBarStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupNotShownOnLongPressingNonEmptyInput() throws Throwable {
        copyStringToClipboard();
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(true);
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingEmptyInput() throws Throwable {
        copyStringToClipboard();
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.clickNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingNonEmptyInput() throws Throwable {
        copyStringToClipboard();
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingOutsideInput() throws Throwable {
        copyStringToClipboard();
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.clickNode(this, mContentViewCore, "plain_text_2");
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnLongPressingOutsideInput() throws Throwable {
        copyStringToClipboard();
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text_2");
        assertWaitForPastePopupStatus(false);
    }

    private void copyStringToClipboard() {
        ClipboardManager clipboardManager =
                (ClipboardManager) getActivity().getSystemService(
                        Context.CLIPBOARD_SERVICE);
        ClipData clip = ClipData.newPlainText("test", "Text to copy");
                clipboardManager.setPrimaryClip(clip);
    }

    private void assertWaitForPastePopupStatus(final boolean show) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return show == mContentViewCore.getPastePopupForTest().isShowing();
            }
        }));
    }

    private void assertWaitForSelectActionBarStatus(
            final boolean show) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return show == mContentViewCore.isSelectActionBarShowing();
            }
        }));
    }
}
