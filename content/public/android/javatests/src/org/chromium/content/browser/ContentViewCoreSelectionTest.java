// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.support.test.filters.SmallTest;
import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.input.ChromiumBaseInputConnection;
import org.chromium.content.browser.input.ImeTestUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.Callable;

/**
 * Integration tests for text selection-related behavior.
 */
public class ContentViewCoreSelectionTest extends ContentShellTestBase {
    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\""
            + "content=\"width=device-width, initial-scale=1.1, maximum-scale=1.5\" /></head>"
            + "<body><form action=\"about:blank\">"
            + "<input id=\"empty_input_text\" type=\"text\" />"
            + "<br/><input id=\"input_text\" type=\"text\" value=\"SampleInputText\" />"
            + "<br/><textarea id=\"textarea\" rows=\"2\" cols=\"20\">SampleTextArea</textarea>"
            + "<br/><input id=\"password\" type=\"password\" value=\"SamplePassword\" size=\"10\"/>"
            + "<br/><p><span id=\"plain_text_1\">SamplePlainTextOne</span></p>"
            + "<br/><p><span id=\"plain_text_2\">SamplePlainTextTwo</span></p>"
            + "<br/><input id=\"disabled_text\" type=\"text\" disabled value=\"Sample Text\" />"
            + "</form></body></html>");
    private ContentViewCore mContentViewCore;
    private SelectionPopupController mSelectionPopupController;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        launchContentShellWithUrl(DATA_URL);
        waitForActiveShellToBeDoneLoading();

        mContentViewCore = getContentViewCore();
        mSelectionPopupController = mContentViewCore.getSelectionPopupControllerForTesting();
        waitForSelectActionBarVisible(false);
        waitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextSelection"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectionClearedAfterLossOfFocus() throws Throwable {
        requestFocusOnUiThread(true);

        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);

        requestFocusOnUiThread(false);
        waitForSelectActionBarVisible(false);
        assertFalse(mSelectionPopupController.hasSelection());

        requestFocusOnUiThread(true);
        waitForSelectActionBarVisible(false);
        assertFalse(mSelectionPopupController.hasSelection());
    }

    @SmallTest
    @Feature({"TextSelection"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectionPreservedAfterLossOfFocusIfRequested() throws Throwable {
        requestFocusOnUiThread(true);

        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());

        mContentViewCore.preserveSelectionOnNextLossOfFocus();
        requestFocusOnUiThread(false);
        waitForSelectActionBarVisible(false);
        assertTrue(mSelectionPopupController.hasSelection());

        requestFocusOnUiThread(true);
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());

        // Losing focus yet again should properly clear the selection.
        requestFocusOnUiThread(false);
        waitForSelectActionBarVisible(false);
        assertFalse(mSelectionPopupController.hasSelection());
    }

    @SmallTest
    @Feature({"TextSelection"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectionPreservedAfterReshown() throws Throwable {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());

        setVisibileOnUiThread(false);
        waitForSelectActionBarVisible(false);
        assertTrue(mSelectionPopupController.hasSelection());

        setVisibileOnUiThread(true);
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
    }

    @SmallTest
    @Feature({"TextSelection"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectionPreservedAfterReattached() throws Throwable {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());

        setAttachedOnUiThread(false);
        waitForSelectActionBarVisible(false);
        assertTrue(mSelectionPopupController.hasSelection());

        setAttachedOnUiThread(true);
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
    }

    /*
    @SmallTest
    @Feature({"TextInput"})
    */
    @DisabledTest(message = "https://crbug.com/592428")
    public void testPastePopupNotShownOnLongPressingNonEmptyInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        waitForSelectActionBarVisible(true);
        waitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingEmptyInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.clickNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingNonEmptyInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.clickNode(mContentViewCore, "input_text");
        waitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingOutsideInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.clickNode(mContentViewCore, "plain_text_2");
        waitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnLongPressingOutsideInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.longPressNode(mContentViewCore, "plain_text_2");
        waitForPastePopupStatus(false);
    }

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

    /*
    @SmallTest
    @Feature({"TextInput"})
    */
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

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testActionBarConfiguredCorrectlyForInput() throws Throwable {
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        assertTrue(mSelectionPopupController.isSelectionEditable());
        assertFalse(mSelectionPopupController.isSelectionPassword());
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testActionBarConfiguredCorrectlyForPassword() throws Throwable {
        DOMUtils.longPressNode(mContentViewCore, "password");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        assertTrue(mSelectionPopupController.isSelectionEditable());
        assertTrue(mSelectionPopupController.isSelectionPassword());
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testActionBarConfiguredCorrectlyForPlainText() throws Throwable {
        DOMUtils.longPressNode(mContentViewCore, "plain_text_1");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        assertFalse(mSelectionPopupController.isSelectionEditable());
        assertFalse(mSelectionPopupController.isSelectionPassword());
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testActionBarConfiguredCorrectlyForTextArea() throws Throwable {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        assertTrue(mSelectionPopupController.isSelectionEditable());
        assertFalse(mSelectionPopupController.isSelectionPassword());
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarPlainTextCopy() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "plain_text_1");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        waitForClipboardContents(mContentViewCore.getContext(), "SamplePlainTextOne");
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarInputCopy() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        waitForClipboardContents(mContentViewCore.getContext(), "SampleInputText");
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarPasswordCopy() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "plain_text_1");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        waitForClipboardContents(mContentViewCore.getContext(), "SamplePlainTextOne");
        DOMUtils.longPressNode(mContentViewCore, "password");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        // Copy option won't be there for Password, hence no change in Clipboard
        // Validating with previous Clipboard content
        waitForClipboardContents(mContentViewCore.getContext(), "SamplePlainTextOne");
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarTextAreaCopy() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        waitForClipboardContents(mContentViewCore.getContext(), "SampleTextArea");
    }

    @SmallTest
    @Feature({"TextSelection"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarPlainTextCut() throws Exception {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "plain_text_1");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertEquals(mSelectionPopupController.getSelectedText(), "SamplePlainTextOne");
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCut();
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        // Cut option won't be available for plain text.
        // Hence validating previous Clipboard content.
        waitForClipboardContents(mContentViewCore.getContext(), "SampleTextToCopy");
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarInputCut() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertEquals(mSelectionPopupController.getSelectedText(), "SampleInputText");
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCut();
        waitForSelectActionBarVisible(false);
        assertFalse(mSelectionPopupController.hasSelection());
        waitForClipboardContents(mContentViewCore.getContext(), "SampleInputText");
        assertEquals(mSelectionPopupController.getSelectedText(), "");
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarPasswordCut() throws Exception {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "password");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCut();
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        // Cut option won't be there for Password, hence no change in Clipboard
        // Validating with previous Clipboard content
        waitForClipboardContents(mContentViewCore.getContext(), "SampleTextToCopy");
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarTextAreaCut() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertEquals(mSelectionPopupController.getSelectedText(), "SampleTextArea");
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCut();
        waitForSelectActionBarVisible(false);
        assertFalse(mSelectionPopupController.hasSelection());
        waitForClipboardContents(mContentViewCore.getContext(), "SampleTextArea");
        assertEquals(mSelectionPopupController.getSelectedText(), "");
    }

    @SmallTest
    @Feature({"TextSelection"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarPlainTextSelectAll() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "plain_text_1");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSelectAll();
        assertTrue(mSelectionPopupController.hasSelection());
        waitForSelectActionBarVisible(true);
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarInputSelectAll() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSelectAll();
        assertTrue(mSelectionPopupController.hasSelection());
        waitForSelectActionBarVisible(true);
        assertEquals(mSelectionPopupController.getSelectedText(), "SampleInputText");
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarPasswordSelectAll() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "password");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSelectAll();
        assertTrue(mSelectionPopupController.hasSelection());
        waitForSelectActionBarVisible(true);
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarTextAreaSelectAll() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSelectAll();
        assertTrue(mSelectionPopupController.hasSelection());
        waitForSelectActionBarVisible(true);
        assertEquals(mSelectionPopupController.getSelectedText(), "SampleTextArea");
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

    /**
     * @SmallTest
     * @Feature({"TextSelection", "TextInput"})
     */
    @DisabledTest(message = "http://crbug.com/606942")
    public void testCursorPositionAfterHidingActionMode() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSelectAll();
        assertTrue(mSelectionPopupController.hasSelection());
        waitForSelectActionBarVisible(true);
        assertEquals(mSelectionPopupController.getSelectedText(), "SampleTextArea");
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

    @SmallTest
    @Feature({"TextSelection"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarPlainTextPaste() throws Exception {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "plain_text_1");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarPaste();
        DOMUtils.longPressNode(mContentViewCore, "plain_text_1");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        // Paste option won't be available for plain text.
        // Hence content won't be changed.
        assertNotSame(mSelectionPopupController.getSelectedText(), "SampleTextToCopy");
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarInputPaste() throws Exception {
        copyStringToClipboard("SampleTextToCopy");

        // Select the input field.
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());

        // Paste into the input field.
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarPaste();
        waitForSelectActionBarVisible(false);
        assertFalse(mSelectionPopupController.hasSelection());

        // Ensure the new text matches the pasted text.
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertEquals("SampleTextToCopy", mSelectionPopupController.getSelectedText());
    }

    /*
    @SmallTest
    @Feature({"TextInput"})
    */
    @DisabledTest(message = "https://crbug.com/592428")
    public void testSelectActionBarPasswordPaste() throws Exception {
        copyStringToClipboard("SamplePassword2");

        // Select the password field.
        DOMUtils.longPressNode(mContentViewCore, "password");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertEquals(mSelectionPopupController.getSelectedText().length(),
                "SamplePassword".length());

        // Paste "SamplePassword2" into the password field, replacing
        // "SamplePassword".
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarPaste();
        waitForSelectActionBarVisible(false);
        assertFalse(mSelectionPopupController.hasSelection());

        // Ensure the new text matches the pasted text. Note that we can't
        // actually compare strings as password field selections only provide
        // a placeholder with the correct length.
        DOMUtils.longPressNode(mContentViewCore, "password");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertEquals(mSelectionPopupController.getSelectedText().length(),
                "SamplePassword2".length());
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarTextAreaPaste() throws Exception {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarPaste();
        DOMUtils.clickNode(mContentViewCore, "plain_text_1");
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertEquals(mSelectionPopupController.getSelectedText(), "SampleTextToCopy");
    }

    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarSearchAndShareLaunchesNewTask() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        waitForSelectActionBarVisible(true);
        assertTrue(mSelectionPopupController.hasSelection());
        assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSearch();
        Intent i = getActivity().getLastSentIntent();
        int new_task_flag = Intent.FLAG_ACTIVITY_NEW_TASK;
        assertEquals(i.getFlags() & new_task_flag, new_task_flag);

        selectActionBarShare();
        i = getActivity().getLastSentIntent();
        assertEquals(i.getFlags() & new_task_flag, new_task_flag);
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

    private void copyStringToClipboard(String string) {
        ClipboardManager clipboardManager =
                (ClipboardManager) getActivity().getSystemService(
                        Context.CLIPBOARD_SERVICE);
        ClipData clip = ClipData.newPlainText("test", string);
        clipboardManager.setPrimaryClip(clip);
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
                return mSelectionPopupController.isInsertion();
            }
        }));
    }
}
