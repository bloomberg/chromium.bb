// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.text.TextUtils;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.input.ChromiumBaseInputConnection;
import org.chromium.content.browser.input.ImeTestUtils;
import org.chromium.content.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.concurrent.Callable;

/**
 * Integration tests for text selection-related behavior.
 */
@RunWith(ContentJUnit4ClassRunner.class)
public class ContentViewCoreSelectionTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();
    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\""
            + "content=\"width=device-width, initial-scale=1.1, maximum-scale=1.5\" /></head>"
            + "<body><form action=\"about:blank\">"
            + "<input id=\"empty_input_text\" type=\"text\" />"
            + "<br/><input id=\"whitespace_input_text\" type=\"text\" value=\" \" />"
            + "<br/><input id=\"input_text\" type=\"text\" value=\"SampleInputText\" />"
            + "<br/><textarea id=\"textarea\" rows=\"2\" cols=\"20\">SampleTextArea</textarea>"
            + "<br/><input id=\"password\" type=\"password\" value=\"SamplePassword\" size=\"10\"/>"
            + "<br/><p><span id=\"smart_selection\">1600 Amphitheatre Parkway</span></p>"
            + "<br/><p><span id=\"plain_text_1\">SamplePlainTextOne</span></p>"
            + "<br/><p><span id=\"plain_text_2\">SamplePlainTextTwo</span></p>"
            + "<br/><input id=\"disabled_text\" type=\"text\" disabled value=\"Sample Text\" />"
            + "<br/><div id=\"rich_div\" contentEditable=\"true\" >Rich Editor</div>"
            + "</form></body></html>");
    private ContentViewCore mContentViewCore;
    private SelectionPopupController mSelectionPopupController;

    private static class TestSelectionClient implements SelectionClient {
        private SelectionClient.Result mResult;
        private SelectionClient.ResultCallback mResultCallback;

        @Override
        public void onSelectionChanged(String selection) {}

        @Override
        public void onSelectionEvent(int eventType, float posXPix, float poxYPix) {}

        @Override
        public void showUnhandledTapUIIfNeeded(int x, int y) {}

        @Override
        public void selectWordAroundCaretAck(boolean didSelect, int startAdjust, int endAdjust) {}

        @Override
        public boolean requestSelectionPopupUpdates(boolean shouldSuggest) {
            ThreadUtils.postOnUiThread(() -> mResultCallback.onClassified(mResult));
            return true;
        }

        @Override
        public void cancelAllRequests() {}

        public void setResult(SelectionClient.Result result) {
            mResult = result;
        }

        public void setResultCallback(SelectionClient.ResultCallback callback) {
            mResultCallback = callback;
        }
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchContentShellWithUrl(DATA_URL);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

        mContentViewCore = mActivityTestRule.getContentViewCore();
        mSelectionPopupController = mContentViewCore.getSelectionPopupControllerForTesting();
        waitForSelectActionBarVisible(false);
        waitForPastePopupStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectionClearedAfterLossOfFocus() throws Throwable {
        requestFocusOnUiThread(true);

        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);

        requestFocusOnUiThread(false);
        waitForSelectActionBarVisible(false);
        Assert.assertFalse(mSelectionPopupController.hasSelection());

        requestFocusOnUiThread(true);
        waitForSelectActionBarVisible(false);
        Assert.assertFalse(mSelectionPopupController.hasSelection());
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionPreservedAfterLossOfFocusIfRequested() throws Throwable {
        requestFocusOnUiThread(true);

        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        mContentViewCore.preserveSelectionOnNextLossOfFocus();
        requestFocusOnUiThread(false);
        waitForSelectActionBarVisible(false);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        requestFocusOnUiThread(true);
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        // Losing focus yet again should properly clear the selection.
        requestFocusOnUiThread(false);
        waitForSelectActionBarVisible(false);
        Assert.assertFalse(mSelectionPopupController.hasSelection());
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionPreservedAfterReshown() throws Throwable {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        setVisibileOnUiThread(false);
        waitForSelectActionBarVisible(false);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        setVisibileOnUiThread(true);
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionPreservedAfterReattached() throws Throwable {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        setAttachedOnUiThread(false);
        waitForSelectActionBarVisible(false);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        setAttachedOnUiThread(true);
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupNotShownOnLongPressingNonEmptyInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        waitForSelectActionBarVisible(true);
        waitForPastePopupStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingEmptyInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.clickNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingNonEmptyInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.clickNode(mContentViewCore, "input_text");
        waitForPastePopupStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingOutsideInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.clickNode(mContentViewCore, "plain_text_2");
        waitForPastePopupStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnLongPressingOutsideInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.longPressNode(mContentViewCore, "plain_text_2");
        waitForPastePopupStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupNotShownOnLongPressingDisabledInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        waitForInsertion(true);
        DOMUtils.longPressNode(mContentViewCore, "disabled_text");
        waitForPastePopupStatus(false);
        waitForInsertion(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupNoSelectAllEmptyInput() throws Throwable {
        // Clipboard has to be non-empty for this test to work on SDK < M.
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        waitForInsertion(true);
        Assert.assertFalse(mSelectionPopupController.canSelectAll());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupCanSelectAllNonEmptyInput() throws Throwable {
        // Clipboard has to be non-empty for this test to work on SDK < M.
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "whitespace_input_text");
        waitForPastePopupStatus(true);
        waitForInsertion(true);
        Assert.assertTrue(mSelectionPopupController.canSelectAll());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupPasteAsPlainTextPlainTextRichEditor() throws Throwable {
        if (!BuildInfo.isAtLeastO()) return;
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "rich_div");
        waitForPastePopupStatus(true);
        waitForInsertion(true);
        Assert.assertFalse(mSelectionPopupController.canPasteAsPlainText());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupPasteAsPlainTextPlainTextNormalEditor() throws Throwable {
        if (!BuildInfo.isAtLeastO()) return;
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        waitForInsertion(true);
        Assert.assertFalse(mSelectionPopupController.canPasteAsPlainText());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupPasteAsPlainTextHtmlTextRichEditor() throws Throwable {
        if (!BuildInfo.isAtLeastO()) return;
        copyHtmlToClipboard("SampleTextToCopy", "<span style=\"color: red;\">HTML</span>");
        DOMUtils.longPressNode(mContentViewCore, "rich_div");
        waitForPastePopupStatus(true);
        waitForInsertion(true);
        Assert.assertTrue(mSelectionPopupController.canPasteAsPlainText());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupPasteAsPlainTextHtmlTextNormalEditor() throws Throwable {
        if (!BuildInfo.isAtLeastO()) return;
        copyHtmlToClipboard("SampleTextToCopy", "<span style=\"color: red;\">HTML</span>");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        waitForInsertion(true);
        Assert.assertFalse(mSelectionPopupController.canPasteAsPlainText());
    }

    @Test
    @MediumTest
    @Feature({"TextInput", "SmartSelection"})
    public void testSmartSelectionNormalFlow() throws Throwable {
        SelectionClient.Result result = new SelectionClient.Result();
        result.startAdjust = -5;
        result.endAdjust = 8;
        result.label = "Maps";

        TestSelectionClient client = new TestSelectionClient();
        client.setResult(result);
        client.setResultCallback(mSelectionPopupController.getResultCallback());

        mSelectionPopupController.setSelectionClient(client);

        DOMUtils.longPressNode(mContentViewCore, "smart_selection");
        waitForSelectActionBarVisible(true);

        SelectionClient.Result returnResult = mSelectionPopupController.getClassificationResult();
        Assert.assertEquals(-5, returnResult.startAdjust);
        Assert.assertEquals(8, returnResult.endAdjust);
        Assert.assertEquals("Maps", returnResult.label);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "https://crbug.com/592428")
    public void testPastePopupDismissedOnDestroy() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mContentViewCore.destroy();
            }
        });
        waitForPastePopupStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testActionBarConfiguredCorrectlyForInput() throws Throwable {
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        Assert.assertTrue(mSelectionPopupController.isSelectionEditable());
        Assert.assertFalse(mSelectionPopupController.isSelectionPassword());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testActionBarConfiguredCorrectlyForPassword() throws Throwable {
        DOMUtils.longPressNode(mContentViewCore, "password");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        Assert.assertTrue(mSelectionPopupController.isSelectionEditable());
        Assert.assertTrue(mSelectionPopupController.isSelectionPassword());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testActionBarConfiguredCorrectlyForPlainText() throws Throwable {
        DOMUtils.longPressNode(mContentViewCore, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        Assert.assertFalse(mSelectionPopupController.isSelectionEditable());
        Assert.assertFalse(mSelectionPopupController.isSelectionPassword());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testActionBarConfiguredCorrectlyForTextArea() throws Throwable {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        Assert.assertTrue(mSelectionPopupController.isSelectionEditable());
        Assert.assertFalse(mSelectionPopupController.isSelectionPassword());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPlainTextCopy() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        waitForClipboardContents(mContentViewCore.getContext(), "SamplePlainTextOne");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarInputCopy() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        waitForClipboardContents(mContentViewCore.getContext(), "SampleInputText");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPasswordCopy() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        waitForClipboardContents(mContentViewCore.getContext(), "SamplePlainTextOne");
        DOMUtils.longPressNode(mContentViewCore, "password");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        // Copy option won't be there for Password, hence no change in Clipboard
        // Validating with previous Clipboard content
        waitForClipboardContents(mContentViewCore.getContext(), "SamplePlainTextOne");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarTextAreaCopy() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        waitForClipboardContents(mContentViewCore.getContext(), "SampleTextArea");
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectActionBarPlainTextCut() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SamplePlainTextOne");
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCut();
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        // Cut option won't be available for plain text.
        // Hence validating previous Clipboard content.
        waitForClipboardContents(mContentViewCore.getContext(), "SampleTextToCopy");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarInputCut() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SampleInputText");
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCut();
        waitForSelectActionBarVisible(false);
        Assert.assertFalse(mSelectionPopupController.hasSelection());
        waitForClipboardContents(mContentViewCore.getContext(), "SampleInputText");
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPasswordCut() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "password");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCut();
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        // Cut option won't be there for Password, hence no change in Clipboard
        // Validating with previous Clipboard content
        waitForClipboardContents(mContentViewCore.getContext(), "SampleTextToCopy");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarTextAreaCut() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SampleTextArea");
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCut();
        waitForSelectActionBarVisible(false);
        Assert.assertFalse(mSelectionPopupController.hasSelection());
        waitForClipboardContents(mContentViewCore.getContext(), "SampleTextArea");
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "");
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectActionBarPlainTextSelectAll() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSelectAll();
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        waitForSelectActionBarVisible(true);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarInputSelectAll() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSelectAll();
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        waitForSelectActionBarVisible(true);
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SampleInputText");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPasswordSelectAll() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "password");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSelectAll();
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        waitForSelectActionBarVisible(true);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarTextAreaSelectAll() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSelectAll();
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        waitForSelectActionBarVisible(true);
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SampleTextArea");
    }

    private CharSequence getTextBeforeCursor(final int length, final int flags) {
        final ChromiumBaseInputConnection connection =
                mContentViewCore.getImeAdapterForTest().getInputConnectionForTest();
        return ImeTestUtils.runBlockingOnHandlerNoException(
                connection.getHandler(), new Callable<CharSequence>() {
                    @Override
                    public CharSequence call() throws Exception {
                        return connection.getTextBeforeCursor(length, flags);
                    }
                });
    }

    @Test
    @SmallTest
    @Feature({"TextSelection", "TextInput"})
    @DisabledTest(message = "http://crbug.com/606942")
    public void testCursorPositionAfterHidingActionMode() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSelectAll();
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        waitForSelectActionBarVisible(true);
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SampleTextArea");
        hideSelectActionMode();
        waitForSelectActionBarVisible(false);
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals("SampleTextArea", new Callable<CharSequence>() {
                    @Override
                    public CharSequence call() {
                        return getTextBeforeCursor(50, 0);
                    }
                }));
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectActionBarPlainTextPaste() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarPaste();
        DOMUtils.longPressNode(mContentViewCore, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        // Paste option won't be available for plain text.
        // Hence content won't be changed.
        Assert.assertNotSame(mSelectionPopupController.getSelectedText(), "SampleTextToCopy");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarInputPaste() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");

        // Select the input field.
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        // Paste into the input field.
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarPaste();
        waitForSelectActionBarVisible(false);
        Assert.assertFalse(mSelectionPopupController.hasSelection());

        // Ensure the new text matches the pasted text.
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertEquals("SampleTextToCopy", mSelectionPopupController.getSelectedText());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPasswordPaste() throws Throwable {
        copyStringToClipboard("SamplePassword2");

        // Select the password field.
        DOMUtils.longPressNode(mContentViewCore, "password");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertEquals(
                mSelectionPopupController.getSelectedText().length(), "SamplePassword".length());

        // Paste "SamplePassword2" into the password field, replacing
        // "SamplePassword".
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarPaste();
        waitForSelectActionBarVisible(false);
        Assert.assertFalse(mSelectionPopupController.hasSelection());

        // Ensure the new text matches the pasted text. Note that we can't
        // actually compare strings as password field selections only provide
        // a placeholder with the correct length.
        DOMUtils.longPressNode(mContentViewCore, "password");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertEquals(
                mSelectionPopupController.getSelectedText().length(), "SamplePassword2".length());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarTextAreaPaste() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarPaste();
        DOMUtils.clickNode(mContentViewCore, "plain_text_1");
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SampleTextToCopy");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarSearchAndShareLaunchesNewTask() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSearch();
        Intent i = mActivityTestRule.getActivity().getLastSentIntent();
        int new_task_flag = Intent.FLAG_ACTIVITY_NEW_TASK;
        Assert.assertEquals(i.getFlags() & new_task_flag, new_task_flag);

        selectActionBarShare();
        i = mActivityTestRule.getActivity().getLastSentIntent();
        Assert.assertEquals(i.getFlags() & new_task_flag, new_task_flag);
    }

    private void selectActionBarPaste() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSelectionPopupController.paste();
            }
        });
    }

    private void selectActionBarSelectAll() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSelectionPopupController.selectAll();
            }
        });
    }

    private void selectActionBarCut() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSelectionPopupController.cut();
            }
        });
    }

    private void selectActionBarCopy() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSelectionPopupController.copy();
            }
        });
    }

    private void selectActionBarSearch() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSelectionPopupController.search();
            }
        });
    }

    private void selectActionBarShare() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mSelectionPopupController.share();
            }
        });
    }

    private void hideSelectActionMode() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mContentViewCore.destroySelectActionMode();
            }
        });
    }

    private void waitForClipboardContents(final Context context, final String expectedContents) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ClipboardManager clipboardManager =
                        (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
                ClipData clip = clipboardManager.getPrimaryClip();
                return clip != null && clip.getItemCount() == 1
                        && TextUtils.equals(clip.getItemAt(0).getText(), expectedContents);
            }
        });
    }

    private void waitForSelectActionBarVisible(final boolean visible) {
        CriteriaHelper.pollUiThread(Criteria.equals(visible, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mContentViewCore.isSelectActionBarShowing();
            }
        }));
    }

    private void setVisibileOnUiThread(final boolean show) {
        final ContentViewCore contentViewCore = mContentViewCore;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (show) {
                    contentViewCore.onShow();
                } else {
                    contentViewCore.onHide();
                }
            }
        });
    }

    private void setAttachedOnUiThread(final boolean attached) {
        final ContentViewCore contentViewCore = mContentViewCore;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (attached) {
                    contentViewCore.onAttachedToWindow();
                } else {
                    contentViewCore.onDetachedFromWindow();
                }
            }
        });
    }

    private void requestFocusOnUiThread(final boolean gainFocus) {
        final ContentViewCore contentViewCore = mContentViewCore;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                contentViewCore.onFocusChanged(gainFocus, true);
            }
        });
    }

    private void copyStringToClipboard(final String string) throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ClipboardManager clipboardManager =
                        (ClipboardManager) mActivityTestRule.getActivity().getSystemService(
                                Context.CLIPBOARD_SERVICE);
                ClipData clip = ClipData.newPlainText("test", string);
                clipboardManager.setPrimaryClip(clip);
            }
        });
    }

    private void copyHtmlToClipboard(final String plainText, final String htmlText)
            throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ClipboardManager clipboardManager =
                        (ClipboardManager) mActivityTestRule.getActivity().getSystemService(
                                Context.CLIPBOARD_SERVICE);
                ClipData clip = ClipData.newHtmlText("html", plainText, htmlText);
                clipboardManager.setPrimaryClip(clip);
            }
        });
    }

    private void waitForPastePopupStatus(final boolean show) {
        CriteriaHelper.pollUiThread(Criteria.equals(show, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mSelectionPopupController.isPastePopupShowing();
            }
        }));
    }

    private void waitForInsertion(final boolean show) {
        CriteriaHelper.pollUiThread(Criteria.equals(show, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mSelectionPopupController.isInsertionForTesting();
            }
        }));
    }
}
