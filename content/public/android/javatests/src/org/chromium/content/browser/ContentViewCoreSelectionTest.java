// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
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
            + "<br/><input id=\"input_password\" type=\"password\" value=\"SamplePassword\" />"
            + "<br/><p><span id=\"plain_text_1\">SamplePlainTextOne</span></p>"
            + "<br/><p><span id=\"plain_text_2\">SamplePlainTextTwo</span></p>"
            + "<br/><input id=\"readonly_text\" type=\"text\" readonly value=\"Sample Text\"/>"
            + "<br/><input id=\"disabled_text\" type=\"text\" disabled value=\"Sample Text\" />"
            + "</form></body></html>");
    private ContentViewCore mContentViewCore;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        launchContentShellWithUrl(DATA_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());

        mContentViewCore = getContentViewCore();
        assertWaitForPageScaleFactorMatch(1.1f);
        assertWaitForSelectActionBarVisible(false);
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionClearedAfterLossOfFocus() throws Throwable {
        requestFocusOnUiThread(true);

        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarVisible(true);

        requestFocusOnUiThread(false);
        assertWaitForSelectActionBarVisible(false);
        assertFalse(mContentViewCore.hasSelection());

        requestFocusOnUiThread(true);
        assertWaitForSelectActionBarVisible(false);
        assertFalse(mContentViewCore.hasSelection());
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionPreservedAfterLossOfFocusIfRequested() throws Throwable {
        requestFocusOnUiThread(true);

        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());

        mContentViewCore.preserveSelectionOnNextLossOfFocus();
        requestFocusOnUiThread(false);
        assertWaitForSelectActionBarVisible(false);
        assertTrue(mContentViewCore.hasSelection());

        requestFocusOnUiThread(true);
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());

        // Losing focus yet again should properly clear the selection.
        requestFocusOnUiThread(false);
        assertWaitForSelectActionBarVisible(false);
        assertFalse(mContentViewCore.hasSelection());
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionPreservedAfterReshown() throws Throwable {
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());

        setVisibileOnUiThread(false);
        assertWaitForSelectActionBarVisible(false);
        assertTrue(mContentViewCore.hasSelection());

        setVisibileOnUiThread(true);
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionPreservedAfterReattached() throws Throwable {
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());

        setAttachedOnUiThread(false);
        assertWaitForSelectActionBarVisible(false);
        assertTrue(mContentViewCore.hasSelection());

        setAttachedOnUiThread(true);
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupNotShownOnLongPressingNonEmptyInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarVisible(true);
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingEmptyInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.clickNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingNonEmptyInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingOutsideInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.clickNode(this, mContentViewCore, "plain_text_2");
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnLongPressingOutsideInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text_2");
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupNotShownOnLongPressingReadOnlyInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        assertTrue(mContentViewCore.hasInsertion());
        DOMUtils.longPressNode(this, mContentViewCore, "readonly_text");
        assertWaitForPastePopupStatus(false);
        assertFalse(mContentViewCore.hasInsertion());
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupNotShownOnLongPressingDisabledInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        assertTrue(mContentViewCore.hasInsertion());
        DOMUtils.longPressNode(this, mContentViewCore, "disabled_text");
        assertWaitForPastePopupStatus(false);
        assertFalse(mContentViewCore.hasInsertion());
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testActionBarConfiguredCorrectlyForInput() throws Throwable {
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        assertTrue(mContentViewCore.getSelectActionHandler().isSelectionEditable());
        assertFalse(mContentViewCore.getSelectActionHandler().isSelectionPassword());
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testActionBarConfiguredCorrectlyForPassword() throws Throwable {
        DOMUtils.longPressNode(this, mContentViewCore, "input_password");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        assertTrue(mContentViewCore.getSelectActionHandler().isSelectionEditable());
        assertTrue(mContentViewCore.getSelectActionHandler().isSelectionPassword());
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testActionBarConfiguredCorrectlyForPlainText() throws Throwable {
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text_1");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        assertFalse(mContentViewCore.getSelectActionHandler().isSelectionEditable());
        assertFalse(mContentViewCore.getSelectActionHandler().isSelectionPassword());
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testActionBarConfiguredCorrectlyForTextArea() throws Throwable {
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        assertTrue(mContentViewCore.getSelectActionHandler().isSelectionEditable());
        assertFalse(mContentViewCore.getSelectActionHandler().isSelectionPassword());
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPlainTextCopy() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text_1");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarCopy();
        assertClipboardContents(mContentViewCore.getContext(), "SamplePlainTextOne");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarInputCopy() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarCopy();
        assertClipboardContents(mContentViewCore.getContext(), "SampleInputText");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPasswordCopy() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text_1");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarCopy();
        assertClipboardContents(mContentViewCore.getContext(), "SamplePlainTextOne");
        DOMUtils.longPressNode(this, mContentViewCore, "input_password");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarCopy();
        // Copy option won't be there for Password, hence no change in Clipboard
        // Validating with previous Clipboard content
        assertClipboardContents(mContentViewCore.getContext(), "SamplePlainTextOne");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarTextAreaCopy() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarCopy();
        assertClipboardContents(mContentViewCore.getContext(), "SampleTextArea");
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectActionBarPlainTextCut() throws Exception {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text_1");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertEquals(mContentViewCore.getSelectedText(), "SamplePlainTextOne");
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarCut();
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        // Cut option won't be available for plain text.
        // Hence validating previous Clipboard content.
        assertClipboardContents(mContentViewCore.getContext(), "SampleTextToCopy");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarInputCut() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertEquals(mContentViewCore.getSelectedText(), "SampleInputText");
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarCut();
        assertWaitForSelectActionBarVisible(false);
        assertFalse(mContentViewCore.hasSelection());
        assertClipboardContents(mContentViewCore.getContext(), "SampleInputText");
        assertEquals(mContentViewCore.getSelectedText(), "");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPasswordCut() throws Exception {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(this, mContentViewCore, "input_password");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarCut();
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        // Cut option won't be there for Password, hence no change in Clipboard
        // Validating with previous Clipboard content
        assertClipboardContents(mContentViewCore.getContext(), "SampleTextToCopy");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarTextAreaCut() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertEquals(mContentViewCore.getSelectedText(), "SampleTextArea");
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarCut();
        assertWaitForSelectActionBarVisible(false);
        assertFalse(mContentViewCore.hasSelection());
        assertClipboardContents(mContentViewCore.getContext(), "SampleTextArea");
        assertEquals(mContentViewCore.getSelectedText(), "");
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectActionBarPlainTextSelectAll() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text_1");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarSelectAll();
        assertTrue(mContentViewCore.hasSelection());
        assertWaitForSelectActionBarVisible(true);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarInputSelectAll() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarSelectAll();
        assertTrue(mContentViewCore.hasSelection());
        assertWaitForSelectActionBarVisible(true);
        assertEquals(mContentViewCore.getSelectedText(), "SampleInputText");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPasswordSelectAll() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "input_password");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarSelectAll();
        assertTrue(mContentViewCore.hasSelection());
        assertWaitForSelectActionBarVisible(true);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarTextAreaSelectAll() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarSelectAll();
        assertTrue(mContentViewCore.hasSelection());
        assertWaitForSelectActionBarVisible(true);
        assertEquals(mContentViewCore.getSelectedText(), "SampleTextArea");
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectActionBarPlainTextPaste() throws Exception {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text_1");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarPaste();
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text_1");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        // Paste option won't be available for plain text.
        // Hence content won't be changed.
        assertNotSame(mContentViewCore.getSelectedText(), "SampleTextToCopy");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarInputPaste() throws Exception {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarPaste();
        DOMUtils.clickNode(this, mContentViewCore, "plain_text_1");
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertEquals(mContentViewCore.getSelectedText(), "SampleTextToCopy");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPasswordPaste() throws Exception {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(this, mContentViewCore, "input_password");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarPaste();
        DOMUtils.clickNode(this, mContentViewCore, "plain_text_1");
        DOMUtils.longPressNode(this, mContentViewCore, "input_password");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotSame(mContentViewCore.getSelectedText(), "SampleTextToCopy");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarTextAreaPaste() throws Exception {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertNotNull(mContentViewCore.getSelectActionHandler());
        selectActionBarPaste();
        DOMUtils.clickNode(this, mContentViewCore, "plain_text_1");
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
        assertEquals(mContentViewCore.getSelectedText(), "SampleTextToCopy");
    }

    private void selectActionBarPaste() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mContentViewCore.getSelectActionHandler().paste();
            }
        });
    }

    private void selectActionBarSelectAll() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mContentViewCore.getSelectActionHandler().selectAll();
            }
        });
    }

    private void selectActionBarCut() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mContentViewCore.getSelectActionHandler().cut();
            }
        });
    }

    private void selectActionBarCopy() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mContentViewCore.getSelectActionHandler().copy();
            }
        });
    }

    private void assertClipboardContents(final Context context, final String expectedContents)
            throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        ClipboardManager clipboardManager =
                                (ClipboardManager) context.getSystemService(
                                        Context.CLIPBOARD_SERVICE);
                        ClipData clip = clipboardManager.getPrimaryClip();
                        return clip != null && clip.getItemCount() == 1
                                && TextUtils.equals(clip.getItemAt(0).getText(), expectedContents);
                    }
                });
            }
        }));
    }

    private void assertWaitForSelectActionBarVisible(
            final boolean visible) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return visible == mContentViewCore.isSelectActionBarShowing();
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
                contentViewCore.onFocusChanged(gainFocus);
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

    private void assertWaitForPastePopupStatus(final boolean show) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return show == mContentViewCore.getPastePopupForTest().isShowing();
            }
        }));
    }
}
