// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Handler;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.text.InputType;
import android.text.TextUtils;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.SelectionPopupController;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_shell_apk.ContentShellTestBase;
import org.chromium.ui.base.ime.TextInputType;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * IME (input method editor) and text input tests.
 */
public class ImeTest extends ContentShellTestBase {
    protected ChromiumBaseInputConnection mConnection;
    private TestInputConnectionFactory mConnectionFactory;
    private ImeAdapter mImeAdapter;

    private static final String INPUT_FORM_HTML =
            "content/test/data/android/input/input_forms.html";

    private ContentViewCore mContentViewCore;
    private WebContents mWebContents;
    private SelectionPopupController mSelectionPopupController;
    private TestCallbackHelperContainer mCallbackContainer;
    protected TestInputMethodManagerWrapper mInputMethodManagerWrapper;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        startActivityWithTestUrl(INPUT_FORM_HTML);
        mContentViewCore = getContentViewCore();
        mSelectionPopupController = mContentViewCore.getSelectionPopupControllerForTesting();
        mWebContents = getWebContents();

        mInputMethodManagerWrapper = new TestInputMethodManagerWrapper(mContentViewCore) {
            private boolean mExpectsSelectionOutsideComposition;

            @Override
            public void expectsSelectionOutsideComposition() {
                mExpectsSelectionOutsideComposition = true;
            }

            @Override
            public void onUpdateSelection(
                    Range oldSel, Range oldComp, Range newSel, Range newComp) {
                // We expect that selection will be outside composition in some cases. Keyboard
                // app will not finish composition in this case.
                if (mExpectsSelectionOutsideComposition) {
                    mExpectsSelectionOutsideComposition = false;
                    return;
                }
                if (oldComp == null || oldComp.start() == oldComp.end()
                        || newComp.start() == newComp.end()) {
                    return;
                }
                // This emulates keyboard app's behavior that finishes composition when
                // selection is outside composition.
                if (!newSel.intersects(newComp)) {
                    try {
                        finishComposingText();
                    } catch (Exception e) {
                        e.printStackTrace();
                        fail();
                    }
                }
            }
        };
        getImeAdapter().setInputMethodManagerWrapperForTest(mInputMethodManagerWrapper);
        assertEquals(0, mInputMethodManagerWrapper.getShowSoftInputCounter());
        mConnectionFactory =
                new TestInputConnectionFactory(getImeAdapter().getInputConnectionFactoryForTest());
        getImeAdapter().setInputConnectionFactory(mConnectionFactory);

        mCallbackContainer = new TestCallbackHelperContainer(mContentViewCore);
        DOMUtils.waitForNonZeroNodeBounds(mWebContents, "input_text");
        boolean result = DOMUtils.clickNode(mContentViewCore, "input_text");

        assertEquals("Failed to dispatch touch event.", true, result);
        assertWaitForKeyboardStatus(true);

        mConnection = getInputConnection();
        mImeAdapter = getImeAdapter();

        waitForKeyboardStates(1, 0, 1, new Integer[] {TextInputType.TEXT});
        assertEquals(0, mConnectionFactory.getOutAttrs().initialSelStart);
        assertEquals(0, mConnectionFactory.getOutAttrs().initialSelEnd);

        waitForEventLogs("selectionchange");
        clearEventLogs();

        waitAndVerifyUpdateSelection(0, 0, 0, -1, -1);
        resetAllStates();
    }

    private void fullyLoadUrl(final String url) throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().getActiveShell().loadUrl(url);
            }
        });
        waitForActiveShellToBeDoneLoading();
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testKeyboardDismissedWhenNavigating() throws Throwable {
        assertWaitForKeyboardStatus(true);

        // Hide keyboard when loading a new Url.
        fullyLoadUrl(UrlUtils.getIsolatedTestFileUrl(INPUT_FORM_HTML));
        assertWaitForKeyboardStatus(false);

        DOMUtils.clickNode(mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);

        // Hide keyboard when navigating.
        final String code = "document.getElementById(\"link\").click()";
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getContentViewCore().getWebContents(), code);
        assertWaitForKeyboardStatus(false);
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testKeyboardDismissedAfterClickingGo() throws Throwable {
        setComposingText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, 0, 5);

        performGo(mCallbackContainer);

        assertWaitForKeyboardStatus(false);
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testDoesNotHang_getTextAfterKeyboardHides() throws Throwable {
        setComposingText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, 0, 5);

        performGo(mCallbackContainer);

        // This may time out if we do not get the information on time.
        // TODO(changwan): find a way to remove the loop.
        for (int i = 0; i < 100; ++i) {
            getTextBeforeCursor(10, 0);
        }

        assertWaitForKeyboardStatus(false);
    }

    // crbug.com/643519
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testCompositionWithNullTextNotCrash() throws Throwable {
        commitText(null, 1);
        assertTextsAroundCursor("", null, "");

        setComposingText(null, 1);
        assertTextsAroundCursor("", null, "");
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDeleteSurroundingTextWithRangeSelection() throws Throwable {
        commitText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        setSelection(1, 4);
        waitAndVerifyUpdateSelection(1, 1, 4, -1, -1);

        deleteSurroundingText(0, 0);
        assertTextsAroundCursor("h", "ell", "o");

        deleteSurroundingText(1, 1);
        assertTextsAroundCursor("", "ell", "");

        deleteSurroundingText(1, 0);
        assertTextsAroundCursor("", "ell", "");

        deleteSurroundingText(0, 1);
        assertTextsAroundCursor("", "ell", "");
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDeleteSurroundingTextWithCursorSelection() throws Throwable {
        commitText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        setSelection(2, 2);
        waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);

        deleteSurroundingText(0, 0);
        assertTextsAroundCursor("he", null, "llo");

        deleteSurroundingText(1, 1);
        assertTextsAroundCursor("h", null, "lo");

        deleteSurroundingText(1, 0);
        assertTextsAroundCursor("", null, "lo");

        deleteSurroundingText(0, 10);
        assertTextsAroundCursor("", null, "");

        deleteSurroundingText(10, 10);
        assertTextsAroundCursor("", null, "");
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testKeyboardAppFinishesCompositionOnUnexpectedSelectionChange() throws Throwable {
        focusElementAndWaitForStateUpdate("textarea");
        commitText("12345", 1);
        setSelection(3, 3);
        setComposingRegion(2, 3);

        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        waitAndVerifyUpdateSelection(1, 3, 3, -1, -1);
        waitAndVerifyUpdateSelection(2, 3, 3, 2, 3);

        // Unexpected selection change occurs, e.g., the user clicks on an area.
        DOMUtils.clickNode(mContentViewCore, "textarea");
        waitAndVerifyUpdateSelection(3, 5, 5, 2, 3);
        // Keyboard app finishes composition. We emulate this in TestInputMethodManagerWrapper.
        waitAndVerifyUpdateSelection(4, 5, 5, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDeleteSurroundingTextInCodePointsWithRangeSelection() throws Throwable {
        final String trophy = "\uD83C\uDFC6";
        commitText("ab" + trophy + "cdef" + trophy + "gh", 1);
        waitAndVerifyUpdateSelection(0, 12, 12, -1, -1);

        setSelection(6, 8);
        waitAndVerifyUpdateSelection(1, 6, 8, -1, -1);
        assertTextsAroundCursor("ab" + trophy + "cd", "ef", trophy + "gh");

        deleteSurroundingTextInCodePoints(2, 2);
        waitAndVerifyUpdateSelection(2, 4, 6, -1, -1);
        assertTextsAroundCursor("ab" + trophy, "ef", "h");
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDeleteSurroundingTextInCodePointsWithCursorSelection() throws Throwable {
        final String trophy = "\uD83C\uDFC6";
        commitText("ab" + trophy + "cd" + trophy, 1);
        waitAndVerifyUpdateSelection(0, 8, 8, -1, -1);

        setSelection(4, 4);
        waitAndVerifyUpdateSelection(1, 4, 4, -1, -1);
        assertTextsAroundCursor("ab" + trophy, null, "cd" + trophy);

        deleteSurroundingTextInCodePoints(2, 2);
        waitAndVerifyUpdateSelection(2, 1, 1, -1, -1);
        assertTextsAroundCursor("a", null, trophy);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testSetComposingTextForNewCursorPositions() throws Throwable {
        // Cursor is on the right of composing text when newCursorPosition > 0.
        setComposingText("ab", 1);
        waitAndVerifyUpdateSelection(0, 2, 2, 0, 2);

        finishComposingText();
        waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);

        // Cursor exceeds the left boundary.
        setComposingText("cdef", -100);
        waitAndVerifyUpdateSelection(2, 0, 0, 2, 6);

        // Cursor is on the left boundary.
        mInputMethodManagerWrapper.expectsSelectionOutsideComposition();
        setComposingText("cd", -2);
        waitAndVerifyUpdateSelection(3, 0, 0, 2, 4);

        mInputMethodManagerWrapper.expectsSelectionOutsideComposition();
        // Cursor is between the left boundary and the composing text.
        setComposingText("cd", -1);
        waitAndVerifyUpdateSelection(4, 1, 1, 2, 4);

        // Cursor is on the left of composing text.
        setComposingText("cd", 0);
        waitAndVerifyUpdateSelection(5, 2, 2, 2, 4);

        finishComposingText();
        waitAndVerifyUpdateSelection(6, 2, 2, -1, -1);

        // Cursor is on the right of composing text.
        setComposingText("ef", 1);
        waitAndVerifyUpdateSelection(7, 4, 4, 2, 4);

        mInputMethodManagerWrapper.expectsSelectionOutsideComposition();
        // Cursor is between the composing text and the right boundary.
        setComposingText("ef", 2);
        waitAndVerifyUpdateSelection(8, 5, 5, 2, 4);

        mInputMethodManagerWrapper.expectsSelectionOutsideComposition();
        // Cursor is on the right boundary.
        setComposingText("ef", 3);
        waitAndVerifyUpdateSelection(9, 6, 6, 2, 4);

        mInputMethodManagerWrapper.expectsSelectionOutsideComposition();
        // Cursor exceeds the right boundary.
        setComposingText("efgh", 100);
        waitAndVerifyUpdateSelection(10, 8, 8, 2, 6);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testCommitTextForNewCursorPositions() throws Throwable {
        // Cursor is on the left of committing text.
        commitText("ab", 0);
        waitAndVerifyUpdateSelection(0, 0, 0, -1, -1);

        // Cursor is on the right of committing text.
        commitText("cd", 1);
        waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);

        // Cursor is between the committing text and the right boundary.
        commitText("ef", 2);
        waitAndVerifyUpdateSelection(2, 5, 5, -1, -1);

        // Cursor is between the left boundary and the committing text.
        commitText("gh", -3);
        waitAndVerifyUpdateSelection(3, 2, 2, -1, -1);

        // Cursor is on the right boundary.
        commitText("ij", 7);
        waitAndVerifyUpdateSelection(4, 10, 10, -1, -1);

        // Cursor is on the left boundary.
        commitText("kl", -10);
        waitAndVerifyUpdateSelection(5, 0, 0, -1, -1);

        // Cursor exceeds the right boundary.
        commitText("mn", 100);
        waitAndVerifyUpdateSelection(6, 14, 14, -1, -1);

        // Cursor exceeds the left boundary.
        commitText("op", -100);
        waitAndVerifyUpdateSelection(7, 0, 0, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testSetComposingTextWithEmptyText() throws Throwable {
        commitText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        setComposingText("AB", 1);
        waitAndVerifyUpdateSelection(1, 7, 7, 5, 7);

        // With previous composition.
        setComposingText("", -3);
        waitAndVerifyUpdateSelection(2, 2, 2, -1, -1);
        assertTextsAroundCursor("he", null, "llo");

        // Without previous composition.
        setComposingText("", 3);
        waitAndVerifyUpdateSelection(3, 4, 4, -1, -1);
        assertTextsAroundCursor("hell", null, "o");
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testCommitTextWithEmptyText() throws Throwable {
        commitText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        setSelection(2, 2);
        waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);

        setComposingText("world", 1);
        waitAndVerifyUpdateSelection(2, 7, 7, 2, 7);
        // With previous composition.
        commitText("", 2);
        waitAndVerifyUpdateSelection(3, 3, 3, -1, -1);

        // Without previous composition.
        commitText("", -1);
        waitAndVerifyUpdateSelection(4, 2, 2, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testCommitWhileComposingText() throws Throwable {
        setComposingText("h", 1);
        waitAndVerifyUpdateSelection(0, 1, 1, 0, 1);

        setComposingText("he", 1);
        waitAndVerifyUpdateSelection(1, 2, 2, 0, 2);

        setComposingText("hel", 1);
        waitAndVerifyUpdateSelection(2, 3, 3, 0, 3);

        commitText("hel", 1);
        waitAndVerifyUpdateSelection(3, 3, 3, -1, -1);

        setComposingText("lo", 1);
        waitAndVerifyUpdateSelection(4, 5, 5, 3, 5);

        commitText("", 1);
        waitAndVerifyUpdateSelection(5, 3, 3, -1, -1);

        assertTextsAroundCursor("hel", null, "");
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testCommitEnterKeyWhileComposingText() throws Throwable {
        focusElementAndWaitForStateUpdate("textarea");

        setComposingText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, 0, 5);

        // Cancel the current composition and replace it with enter.
        commitText("\n", 1);
        waitAndVerifyUpdateSelection(1, 1, 1, -1, -1);
        // The second new line is not a user visible/editable one, it is a side-effect of Blink
        // using <br> internally. This only happens when \n is at the end.
        assertTextsAroundCursor("\n", null, "\n");

        commitText("world", 1);
        waitAndVerifyUpdateSelection(2, 6, 6, -1, -1);
        assertTextsAroundCursor("\nworld", null, "");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeCopy() throws Exception {
        commitText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        setSelection(2, 5);
        waitAndVerifyUpdateSelection(1, 2, 5, -1, -1);

        copy();
        assertClipboardContents(getActivity(), "llo");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testEnterTextAndRefocus() throws Exception {
        commitText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        restartInput();
        DOMUtils.clickNode(mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);

        assertEquals(5, mConnectionFactory.getOutAttrs().initialSelStart);
        assertEquals(5, mConnectionFactory.getOutAttrs().initialSelEnd);
    }

    @SmallTest
    @DisabledTest(message = "crbug.com/694812")
    @Feature({"TextInput"})
    public void testShowAndHideSoftInput() throws Exception {
        focusElement("input_radio", false);

        // hideSoftKeyboard(), restartInput()
        waitForKeyboardStates(0, 1, 1, new Integer[] {});

        // When input connection is null, we still need to set flags to prevent InputMethodService
        // from entering fullscreen mode and from opening custom UI.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getInputConnection() == null;
            }
        });
        assertTrue(
                (mConnectionFactory.getOutAttrs().imeOptions
                        & (EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_FLAG_NO_EXTRACT_UI))
                != 0);

        // showSoftInput(), restartInput()
        focusElement("input_number1");
        waitForKeyboardStates(1, 1, 2, new Integer[] {TextInputType.NUMBER});
        assertNotNull(mInputMethodManagerWrapper.getInputConnection());

        focusElement("input_number2");
        // Hide should never be called here. Otherwise we will see a flicker. Restarted to
        // reset internal states to handle the new input form.
        waitForKeyboardStates(2, 1, 3, new Integer[] {TextInputType.NUMBER, TextInputType.NUMBER});

        focusElement("input_text");
        // showSoftInput() on input_text. restartInput() on input_number1 due to focus change,
        // and restartInput() on input_text later.
        waitForKeyboardStates(3, 1, 4,
                new Integer[] {TextInputType.NUMBER, TextInputType.NUMBER, TextInputType.TEXT});

        resetUpdateSelectionList();
        setComposingText("a", 1);
        waitAndVerifyUpdateSelection(0, 1, 1, 0, 1);
        resetUpdateSelectionList();

        // JavaScript changes focus.
        String code = "(function() { "
                + "var textarea = document.getElementById('textarea');"
                + "textarea.focus();"
                + "})();";
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getContentViewCore().getWebContents(), code);
        waitAndVerifyUpdateSelection(0, 0, 0, -1, -1);
        resetUpdateSelectionList();

        waitForKeyboardStates(4, 1, 5, new Integer[] {
                TextInputType.NUMBER, TextInputType.NUMBER, TextInputType.TEXT,
                TextInputType.TEXT_AREA});
        assertEquals(0, mConnectionFactory.getOutAttrs().initialSelStart);
        assertEquals(0, mConnectionFactory.getOutAttrs().initialSelEnd);

        setComposingText("aa", 1);
        waitAndVerifyUpdateSelection(0, 2, 2, 0, 2);

        focusElement("input_text");
        waitForKeyboardStates(5, 1, 6, new Integer[] {
                TextInputType.NUMBER, TextInputType.NUMBER, TextInputType.TEXT,
                TextInputType.TEXT_AREA, TextInputType.TEXT});
        assertEquals(1, mConnectionFactory.getOutAttrs().initialSelStart);
        assertEquals(1, mConnectionFactory.getOutAttrs().initialSelEnd);

        focusElement("input_radio", false);
        // hideSoftInput(), restartInput()
        waitForKeyboardStates(5, 2, 7, new Integer[] {
                TextInputType.NUMBER, TextInputType.NUMBER, TextInputType.TEXT,
                TextInputType.TEXT_AREA, TextInputType.TEXT});
    }

    private void assertTextsAroundCursor(
            CharSequence before, CharSequence selected, CharSequence after) throws Exception {
        assertEquals(before, getTextBeforeCursor(100, 0));
        assertEquals(selected, getSelectedText(0));
        assertEquals(after, getTextAfterCursor(100, 0));
    }

    private void waitForKeyboardStates(int show, int hide, int restart, Integer[] history) {
        final String expected = stringifyKeyboardStates(show, hide, restart, history);
        CriteriaHelper.pollUiThread(Criteria.equals(expected, new Callable<String>() {
            @Override
            public String call() {
                return getKeyboardStates();
            }
        }));
    }

    private void resetAllStates() {
        mInputMethodManagerWrapper.reset();
        mConnectionFactory.clearTextInputTypeHistory();
    }

    private String getKeyboardStates() {
        int showCount = mInputMethodManagerWrapper.getShowSoftInputCounter();
        int hideCount = mInputMethodManagerWrapper.getHideSoftInputCounter();
        int restartCount = mInputMethodManagerWrapper.getRestartInputCounter();
        Integer[] history = mConnectionFactory.getTextInputTypeHistory();
        return stringifyKeyboardStates(showCount, hideCount, restartCount, history);
    }

    private String stringifyKeyboardStates(int show, int hide, int restart, Integer[] history) {
        return "show count: " + show + ", hide count: " + hide + ", restart count: " + restart
                + ", input type history: " + Arrays.deepToString(history);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testKeyboardNotDismissedAfterCopySelection() throws Exception {
        commitText("Sample Text", 1);
        waitAndVerifyUpdateSelection(0, 11, 11, -1, -1);

        // Select 'text' part.
        DOMUtils.longPressNode(mContentViewCore, "input_text");

        assertWaitForSelectActionBarStatus(true);

        selectAll();
        copy();
        assertClipboardContents(getActivity(), "Sample Text");
        assertEquals(11, mInputMethodManagerWrapper.getSelection().end());
        assertWaitForKeyboardStatus(true);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeNotDismissedAfterCutSelection() throws Exception {
        commitText("Sample Text", 1);
        waitAndVerifyUpdateSelection(0, 11, 11, -1, -1);
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(true);
        assertWaitForKeyboardStatus(true);
        cut();
        assertWaitForKeyboardStatus(true);
        assertWaitForSelectActionBarStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeNotShownOnLongPressingEmptyInput() throws Exception {
        DOMUtils.focusNode(mWebContents, "input_radio");
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(false);
        commitText("Sample Text", 1);
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarShownOnLongPressingInput() throws Exception {
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(false);
        commitText("Sample Text", 1);
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(true);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testLongPressInputWhileComposingText() throws Exception {
        assertWaitForSelectActionBarStatus(false);
        setComposingText("Sample Text", 1);
        waitAndVerifyUpdateSelection(0, 11, 11, 0, 11);
        DOMUtils.longPressNode(mContentViewCore, "input_text");

        assertWaitForSelectActionBarStatus(true);

        // Long press will first change selection region, and then trigger IME app to show up.
        // See RenderFrameImpl::didChangeSelection() and RenderWidget::didHandleGestureEvent().
        waitAndVerifyUpdateSelection(1, 7, 11, 0, 11);

        // Now IME app wants to finish composing text because an external selection
        // change has been detected. At least Google Latin IME and Samsung IME
        // behave this way.
        finishComposingText();
        waitAndVerifyUpdateSelection(2, 7, 11, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeShownWhenLongPressOnAlreadySelectedText() throws Exception {
        assertWaitForSelectActionBarStatus(false);
        commitText("Sample Text", 1);

        int showCount = mInputMethodManagerWrapper.getShowSoftInputCounter();
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(true);
        assertEquals(showCount + 1, mInputMethodManagerWrapper.getShowSoftInputCounter());

        // Now long press again. Selection region remains the same, but the logic
        // should trigger IME to show up. Note that Android does not provide show /
        // hide status of IME, so we will just check whether showIme() has been triggered.
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        final int newCount = showCount + 2;
        CriteriaHelper.pollUiThread(Criteria.equals(newCount, new Callable<Integer>() {
            @Override
            public Integer call() {
                return mInputMethodManagerWrapper.getShowSoftInputCounter();
            }
        }));
    }

    private void attachPhysicalKeyboard() {
        Configuration hardKeyboardConfig =
                new Configuration(mContentViewCore.getContext().getResources().getConfiguration());
        hardKeyboardConfig.keyboard = Configuration.KEYBOARD_QWERTY;
        hardKeyboardConfig.keyboardHidden = Configuration.KEYBOARDHIDDEN_YES;
        hardKeyboardConfig.hardKeyboardHidden = Configuration.HARDKEYBOARDHIDDEN_NO;
        onConfigurationChanged(hardKeyboardConfig);
    }

    private void detachPhysicalKeyboard() {
        Configuration softKeyboardConfig =
                new Configuration(mContentViewCore.getContext().getResources().getConfiguration());
        softKeyboardConfig.keyboard = Configuration.KEYBOARD_NOKEYS;
        softKeyboardConfig.keyboardHidden = Configuration.KEYBOARDHIDDEN_NO;
        softKeyboardConfig.hardKeyboardHidden = Configuration.HARDKEYBOARDHIDDEN_YES;
        onConfigurationChanged(softKeyboardConfig);
    }

    private void onConfigurationChanged(final Configuration config) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mContentViewCore.onConfigurationChanged(config);
            }
        });
    }

    private void reloadPage() throws Throwable {
        // Reload the page, then focus will be lost and keyboard should be hidden.
        fullyLoadUrl(getContentViewCore().getWebContents().getUrl());
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPhysicalKeyboard_AttachDetach() throws Throwable {
        attachPhysicalKeyboard();
        // We still call showSoftKeyboard, which will be ignored by physical keyboard.
        waitForKeyboardStates(1, 0, 1, new Integer[] {TextInputType.TEXT});
        setComposingText("a", 1);
        waitForKeyboardStates(1, 0, 1, new Integer[] {TextInputType.TEXT});
        detachPhysicalKeyboard();
        assertWaitForKeyboardStatus(true);
        // Now we really show soft keyboard. We also call restartInput when configuration changes.
        waitForKeyboardStates(2, 0, 2, new Integer[] {TextInputType.TEXT, TextInputType.TEXT});

        reloadPage();

        // Depending on the timing, hideSoftInput and restartInput call counts may vary here
        // because render widget gets restarted. But the end result should be the same.
        assertWaitForKeyboardStatus(false);

        detachPhysicalKeyboard();

        try {
            // We should not show soft keyboard here because focus has been lost.
            CriteriaHelper.pollUiThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mInputMethodManagerWrapper.isShowWithoutHideOutstanding();
                }
            });
            fail("Keyboard incorrectly showing");
        } catch (AssertionError e) {
            // TODO(tedchoc): This is horrible and should never timeout to determine success.
        }
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarClearedOnTappingInput() throws Exception {
        commitText("Sample Text", 1);
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        assertWaitForSelectActionBarStatus(true);
        DOMUtils.clickNode(mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarClearedOnTappingOutsideInput() throws Exception {
        commitText("Sample Text", 1);
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        assertWaitForSelectActionBarStatus(true);
        DOMUtils.clickNode(mContentViewCore, "input_radio");
        assertWaitForKeyboardStatus(false);
        assertWaitForSelectActionBarStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeNotShownOnLongPressingDifferentEmptyInputs() throws Exception {
        DOMUtils.focusNode(mWebContents, "input_radio");
        DOMUtils.longPressNode(mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(false);
        DOMUtils.longPressNode(mContentViewCore, "textarea");
        assertWaitForKeyboardStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeStaysOnLongPressingDifferentNonEmptyInputs() throws Exception {
        DOMUtils.focusNode(mWebContents, "input_text");
        assertWaitForKeyboardStatus(true);

        commitText("Sample Text", 1);
        // We should wait to avoid race condition.
        waitAndVerifyUpdateSelection(0, 11, 11, -1, -1);

        DOMUtils.focusNode(mWebContents, "textarea");
        waitAndVerifyUpdateSelection(1, 0, 0, -1, -1);

        commitText("Sample Text", 1);
        waitAndVerifyUpdateSelection(2, 11, 11, -1, -1);

        DOMUtils.longPressNode(mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        assertWaitForSelectActionBarStatus(true);

        DOMUtils.longPressNode(mContentViewCore, "textarea");
        assertWaitForKeyboardStatus(true);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeCut() throws Exception {
        commitText("snarful", 1);
        waitAndVerifyUpdateSelection(0, 7, 7, -1, -1);

        setSelection(1, 5);
        waitAndVerifyUpdateSelection(1, 1, 5, -1, -1);

        cut();
        waitAndVerifyUpdateSelection(2, 1, 1, -1, -1);
        assertTextsAroundCursor("s", null, "ul");
        assertClipboardContents(getActivity(), "narf");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImePaste() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClipboardManager clipboardManager =
                        (ClipboardManager) getActivity().getSystemService(
                                Context.CLIPBOARD_SERVICE);
                clipboardManager.setPrimaryClip(ClipData.newPlainText("blarg", "blarg"));
            }
        });

        paste();
        // Paste is a two step process when there is a non-zero selection.
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        assertTextsAroundCursor("blarg", null, "");

        setSelection(3, 5);
        waitAndVerifyUpdateSelection(1, 3, 5, -1, -1);
        assertTextsAroundCursor("bla", "rg", "");

        paste();
        // Paste is a two step process when there is a non-zero selection.
        waitAndVerifyUpdateSelection(2, 8, 8, -1, -1);
        assertTextsAroundCursor("blablarg", null, "");

        paste();
        waitAndVerifyUpdateSelection(3, 13, 13, -1, -1);
        assertTextsAroundCursor("blablargblarg", null, "");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeSelectAndCollapseSelection() throws Exception {
        commitText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        selectAll();
        waitAndVerifyUpdateSelection(1, 0, 5, -1, -1);

        collapseSelection();
        waitAndVerifyUpdateSelection(2, 5, 5, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testShowImeIfNeeded() throws Throwable {
        // showImeIfNeeded() is now implicitly called by the updated focus
        // heuristic so no need to call explicitly. http://crbug.com/371927
        DOMUtils.focusNode(mWebContents, "input_radio");
        assertWaitForKeyboardStatus(false);

        DOMUtils.focusNode(mWebContents, "input_text");
        assertWaitForKeyboardStatus(true);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testFinishComposingText() throws Throwable {
        focusElementAndWaitForStateUpdate("textarea");

        commitText("hllo", 1);
        waitAndVerifyUpdateSelection(0, 4, 4, -1, -1);

        commitText(" ", 1);
        waitAndVerifyUpdateSelection(1, 5, 5, -1, -1);

        setSelection(1, 1);
        waitAndVerifyUpdateSelection(2, 1, 1, -1, -1);
        assertTextsAroundCursor("h", null, "llo ");

        setComposingRegion(0, 4);
        waitAndVerifyUpdateSelection(3, 1, 1, 0, 4);

        finishComposingText();
        waitAndVerifyUpdateSelection(4, 1, 1, -1, -1);

        commitText("\n", 1);
        waitAndVerifyUpdateSelection(5, 2, 2, -1, -1);
        assertTextsAroundCursor("h\n", null, "llo ");
    }

    // http://crbug.com/445499
    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDeleteText() throws Throwable {
        focusElement("textarea");

        // The calls below are a reflection of what the stock Google Keyboard (Andr
        // when the noted key is touched on screen.
        // H
        setComposingText("h", 1);
        assertEquals("h", getTextBeforeCursor(9, 0));

        // O
        setComposingText("ho", 1);
        assertEquals("ho", getTextBeforeCursor(9, 0));

        setComposingText("h", 1);
        setComposingRegion(0, 1);
        setComposingText("h", 1);
        assertEquals("h", getTextBeforeCursor(9, 0));

        // I
        setComposingText("hi", 1);
        assertEquals("hi", getTextBeforeCursor(9, 0));

        // SPACE
        commitText("hi", 1);
        commitText(" ", 1);
        assertEquals("hi ", getTextBeforeCursor(9, 0));

        // DEL
        deleteSurroundingText(1, 0);
        setComposingRegion(0, 2);
        assertEquals("hi", getTextBeforeCursor(9, 0));

        setComposingText("h", 1);
        assertEquals("h", getTextBeforeCursor(9, 0));

        commitText("", 1);
        assertEquals("", getTextBeforeCursor(9, 0));

        // DEL (on empty input)
        deleteSurroundingText(1, 0); // DEL on empty still sends 1,0
        assertEquals("", getTextBeforeCursor(9, 0));
    }


    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testSwipingText() throws Throwable {
        focusElement("textarea");

        // The calls below are a reflection of what the stock Google Keyboard (Android 4.4) sends
        // when the word is swiped on the soft keyboard.  Exercise care when altering to make sure
        // that the test reflects reality.  If this test breaks, it's possible that code has
        // changed and different calls need to be made instead.
        // "three"
        setComposingText("three", 1);
        assertEquals("three", getTextBeforeCursor(99, 0));

        // "word"
        commitText("three", 1);
        commitText(" ", 1);
        setComposingText("word", 1);
        assertEquals("three word", getTextBeforeCursor(99, 0));

        // "test"
        commitText("word", 1);
        commitText(" ", 1);
        setComposingText("test", 1);
        assertEquals("three word test", getTextBeforeCursor(99, 0));
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDeleteMultiCharacterCodepoint() throws Throwable {
        // This smiley is a multi character codepoint.
        final String smiley = "\uD83D\uDE0A";

        commitText(smiley, 1);
        waitAndVerifyUpdateSelection(0, 2, 2, -1, -1);
        assertTextsAroundCursor(smiley, null, "");

        // DEL, sent via dispatchKeyEvent like it is in Android WebView or a physical keyboard.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));

        waitAndVerifyUpdateSelection(1, 0, 0, -1, -1);

        // Make sure that we accept further typing after deleting the smiley.
        setComposingText("s", 1);
        setComposingText("sm", 1);
        waitAndVerifyUpdateSelection(2, 1, 1, 0, 1);
        waitAndVerifyUpdateSelection(3, 2, 2, 0, 2);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testBackspaceKeycode() throws Throwable {
        focusElement("textarea");

        // H
        commitText("h", 1);
        assertEquals("h", getTextBeforeCursor(9, 0));

        // O
        commitText("o", 1);
        assertEquals("ho", getTextBeforeCursor(9, 0));

        // DEL, sent via dispatchKeyEvent like it is in Android WebView or a physical keyboard.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));

        // DEL
        assertEquals("h", getTextBeforeCursor(9, 0));
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testRepeatBackspaceKeycode() throws Throwable {
        focusElement("textarea");

        // H
        commitText("h", 1);
        assertEquals("h", getTextBeforeCursor(9, 0));

        // O
        commitText("o", 1);
        assertEquals("ho", getTextBeforeCursor(9, 0));

        // Multiple keydowns should each delete one character (this is for physical keyboard
        // key-repeat).
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));

        // DEL
        assertEquals("", getTextBeforeCursor(9, 0));
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testPhysicalKeyboard() throws Throwable {
        focusElementAndWaitForStateUpdate("textarea");

        // Type 'a' using a physical keyboard.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_A));
        waitAndVerifyUpdateSelection(0, 1, 1, -1, -1);

        // Type 'enter' key.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
        waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);
        assertTextsAroundCursor("a\n", null, "\n");

        // Type 'b'.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_B));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_B));
        waitAndVerifyUpdateSelection(2, 3, 3, -1, -1);
        assertTextsAroundCursor("a\nb", null, "");
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testPhysicalKeyboard_AccentKeyCodes() throws Throwable {
        focusElementAndWaitForStateUpdate("textarea");

        // h
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_H));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_H));
        assertEquals("h", getTextBeforeCursor(9, 0));
        waitAndVerifyUpdateSelection(0, 1, 1, -1, -1);

        // ALT-i  (circumflex accent key on virtual keyboard)
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertEquals("hˆ", getTextBeforeCursor(9, 0));
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertEquals("hˆ", getTextBeforeCursor(9, 0));
        waitAndVerifyUpdateSelection(1, 2, 2, 1, 2);

        // o
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_O));
        assertEquals("hô", getTextBeforeCursor(9, 0));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_O));
        assertEquals("hô", getTextBeforeCursor(9, 0));
        waitAndVerifyUpdateSelection(2, 2, 2, -1, -1);

        // ALT-i
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertEquals("hôˆ", getTextBeforeCursor(9, 0));
        waitAndVerifyUpdateSelection(3, 3, 3, 2, 3);

        // ALT-i again should have no effect
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertEquals("hôˆ", getTextBeforeCursor(9, 0));

        // b (cannot be accented, should just appear after)
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_B));
        assertEquals("hôˆb", getTextBeforeCursor(9, 0));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_B));
        assertEquals("hôˆb", getTextBeforeCursor(9, 0));
        int index = 4;
        waitAndVerifyUpdateSelection(index++, 4, 4, -1, -1);

        // ALT-i
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertEquals("hôˆbˆ", getTextBeforeCursor(9, 0));
        waitAndVerifyUpdateSelection(index++, 5, 5, 4, 5);

        // Backspace
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        assertEquals("hôˆb", getTextBeforeCursor(9, 0));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));
        assertEquals("hôˆb", getTextBeforeCursor(9, 0));
        waitAndVerifyUpdateSelection(index++, 4, 4, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testSetComposingRegionOutOfBounds() throws Throwable {
        focusElementAndWaitForStateUpdate("textarea");
        setComposingText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, 0, 5);

        setComposingRegion(0, 0);
        waitAndVerifyUpdateSelection(1, 5, 5, -1, -1);
        setComposingRegion(0, 9);
        waitAndVerifyUpdateSelection(2, 5, 5, 0, 5);
        setComposingRegion(9, 1);
        waitAndVerifyUpdateSelection(3, 5, 5, 1, 5);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testEnterKey_AfterCommitText() throws Throwable {
        focusElementAndWaitForStateUpdate("textarea");

        commitText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
        waitAndVerifyUpdateSelection(1, 6, 6, -1, -1);
        assertTextsAroundCursor("hello\n", null, "\n");

        commitText("world", 1);
        waitAndVerifyUpdateSelection(2, 11, 11, -1, -1);
        assertTextsAroundCursor("hello\nworld", null, "");
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testEnterKey_WhileComposingText() throws Throwable {
        focusElementAndWaitForStateUpdate("textarea");

        setComposingText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, 0, 5);

        // IME app should call this, otherwise enter key should clear the current composition.
        finishComposingText();
        waitAndVerifyUpdateSelection(1, 5, 5, -1, -1);

        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));

        // The second new line is not a user visible/editable one, it is a side-effect of Blink
        // using <br> internally. This only happens when \n is at the end.
        waitAndVerifyUpdateSelection(2, 6, 6, -1, -1);

        commitText("world", 1);
        waitAndVerifyUpdateSelection(3, 11, 11, -1, -1);
        assertTextsAroundCursor("hello\nworld", null, "");
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDpadKeyCodesWhileSwipingText() throws Throwable {
        focusElement("textarea");

        // DPAD_CENTER should cause keyboard to appear
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_CENTER));

        // TODO(changwan): should really check this.
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testNavigateTextWithDpadKeyCodes() throws Throwable {
        focusElementAndWaitForStateUpdate("textarea");

        commitText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_LEFT));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DPAD_LEFT));

        assertTextsAroundCursor("hell", null, "o");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupShowAndHide() throws Throwable {
        commitText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);

        selectAll();
        waitAndVerifyUpdateSelection(1, 0, 5, -1, -1);
        assertTextsAroundCursor("", "hello", "");

        cut();
        waitAndVerifyUpdateSelection(2, 0, 0, -1, -1);
        assertTextsAroundCursor("", null, "");

        DOMUtils.longPressNode(mContentViewCore, "input_text");
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mSelectionPopupController.isPastePopupShowing()
                       && mSelectionPopupController.isInsertion();
            }
        });

        setComposingText("h", 1);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !mSelectionPopupController.isPastePopupShowing();
            }
        });
        assertFalse(mSelectionPopupController.isInsertion());
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectionClearedOnKeyEvent() throws Throwable {
        commitText("Sample Text", 1);
        waitAndVerifyUpdateSelection(0, 11, 11, -1, -1);

        DOMUtils.longPressNode(mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(true);

        setComposingText("h", 1);
        assertWaitForSelectActionBarStatus(false);
        assertFalse(mSelectionPopupController.hasSelection());
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testTextHandlesPreservedWithDpadNavigation() throws Throwable {
        DOMUtils.longPressNode(mContentViewCore, "plain_text");
        assertWaitForSelectActionBarStatus(true);
        assertTrue(mSelectionPopupController.hasSelection());

        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_DOWN));
        assertWaitForSelectActionBarStatus(true);
        assertTrue(mSelectionPopupController.hasSelection());
    }

    @MediumTest
    @Feature({"TextInput"})
    public void testRestartInputWhileComposingText() throws Throwable {
        setComposingText("abc", 1);
        waitAndVerifyUpdateSelection(0, 3, 3, 0, 3);
        restartInput();
        // We don't do anything when input gets restarted. But we depend on Android's
        // InputMethodManager and/or input methods to call finishComposingText() in setting
        // current input connection as active or finishing the current input connection.
        Thread.sleep(1000);
        assertEquals(1, mInputMethodManagerWrapper.getUpdateSelectionList().size());
    }

    @MediumTest
    @Feature({"TextInput"})
    public void testRestartInputKeepsTextAndCursor() throws Exception {
        commitText("ab", 2);
        restartInput();
        assertEquals("ab", getTextBeforeCursor(10, 0));
    }

    @MediumTest
    @Feature({"TextInput"})
    public void testContentEditableEvents_ComposingText() throws Throwable {
        focusElementAndWaitForStateUpdate("contenteditable_event");
        waitForEventLogs("selectionchange,selectionchange");
        clearEventLogs();

        setComposingText("a", 1);
        waitAndVerifyUpdateSelection(0, 1, 1, 0, 1);
        // TODO(changwan): reduce the number of selection changes.
        waitForEventLogs("keydown(229),compositionstart(),compositionupdate(a),input,keyup(229),"
                + "selectionchange,selectionchange");
        clearEventLogs();

        finishComposingText();
        waitAndVerifyUpdateSelection(1, 1, 1, -1, -1);
        waitForEventLogs(
                "compositionend(a)");
    }

    @MediumTest
    @Feature({"TextInput"})
    public void testInputTextEvents_ComposingText() throws Throwable {
        setComposingText("a", 1);
        waitAndVerifyUpdateSelection(0, 1, 1, 0, 1);
        // TODO(changwan): reduce the number of selection changes.
        waitForEventLogs("keydown(229),compositionstart(),compositionupdate(a),"
                + "input,keyup(229),selectionchange,selectionchange");
        clearEventLogs();

        finishComposingText();
        waitAndVerifyUpdateSelection(1, 1, 1, -1, -1);
        waitForEventLogs("compositionend(a)");
    }

    @MediumTest
    @Feature({"TextInput"})
    public void testContentEditableEvents_CommitText() throws Throwable {
        focusElementAndWaitForStateUpdate("contenteditable_event");
        waitForEventLogs("selectionchange,selectionchange");
        clearEventLogs();

        commitText("a", 1);
        waitAndVerifyUpdateSelection(0, 1, 1, -1, -1);

        waitForEventLogs("keydown(229),input,keyup(229),selectionchange");
    }

    @MediumTest
    @Feature({"TextInput"})
    public void testInputTextEvents_CommitText() throws Throwable {
        commitText("a", 1);
        waitAndVerifyUpdateSelection(0, 1, 1, -1, -1);

        waitForEventLogs("keydown(229),input,keyup(229),selectionchange");
    }

    @MediumTest
    @Feature({"TextInput"})
    public void testContentEditableEvents_DeleteSurroundingText() throws Throwable {
        focusElementAndWaitForStateUpdate("contenteditable_event");
        waitForEventLogs("selectionchange,selectionchange");
        clearEventLogs();

        commitText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        waitForEventLogs("keydown(229),input,keyup(229),selectionchange");
        clearEventLogs();

        setSelection(2, 2);
        waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);
        waitForEventLogs("selectionchange");
        clearEventLogs();

        deleteSurroundingText(1, 1);
        waitAndVerifyUpdateSelection(2, 1, 1, -1, -1);
        // TODO(yabinh): It should only fire 1 input and 1 selectionchange events.
        waitForEventLogs("keydown(229),input,input,keyup(229),selectionchange,selectionchange");
    }

    @MediumTest
    @Feature({"TextInput"})
    public void testInputTextEvents_DeleteSurroundingText() throws Throwable {
        commitText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        waitForEventLogs("keydown(229),input,keyup(229),selectionchange");
        clearEventLogs();

        setSelection(2, 2);
        waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);
        waitForEventLogs("selectionchange");
        clearEventLogs();

        deleteSurroundingText(1, 1);
        waitAndVerifyUpdateSelection(2, 1, 1, -1, -1);
        // TODO(yabinh): It should only fire 1 input and 1 selectionchange events.
        waitForEventLogs("keydown(229),input,input,keyup(229),selectionchange,selectionchange");
    }

    @MediumTest
    @Feature({"TextInput"})
    public void testContentEditableEvents_DeleteSurroundingTextInCodePoints() throws Throwable {
        focusElementAndWaitForStateUpdate("contenteditable_event");
        waitForEventLogs("selectionchange,selectionchange");
        clearEventLogs();

        commitText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        waitForEventLogs("keydown(229),input,keyup(229),selectionchange");
        clearEventLogs();

        setSelection(2, 2);
        waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);
        waitForEventLogs("selectionchange");
        clearEventLogs();

        deleteSurroundingTextInCodePoints(1, 1);
        waitAndVerifyUpdateSelection(2, 1, 1, -1, -1);
        // TODO(yabinh): It should only fire 1 input and 1 selectionchange events.
        waitForEventLogs("keydown(229),input,input,keyup(229),selectionchange,selectionchange");
    }

    @MediumTest
    @Feature({"TextInput"})
    public void testInputTextEvents_DeleteSurroundingTextInCodePoints() throws Throwable {
        commitText("hello", 1);
        waitAndVerifyUpdateSelection(0, 5, 5, -1, -1);
        waitForEventLogs("keydown(229),input,keyup(229),selectionchange");
        clearEventLogs();

        setSelection(2, 2);
        waitAndVerifyUpdateSelection(1, 2, 2, -1, -1);
        waitForEventLogs("selectionchange");
        clearEventLogs();

        deleteSurroundingTextInCodePoints(1, 1);
        waitAndVerifyUpdateSelection(2, 1, 1, -1, -1);
        // TODO(yabinh): It should only fire 1 input and 1 selectionchange events.
        waitForEventLogs("keydown(229),input,input,keyup(229),selectionchange,selectionchange");
    }

    @MediumTest
    @Feature({"TextInput"})
    public void testGetCursorCapsMode() throws Throwable {
        focusElementAndWaitForStateUpdate("contenteditable_event");
        commitText("Hello World", 1);
        waitAndVerifyUpdateSelection(0, 11, 11, -1, -1);
        assertEquals(0,
                getCursorCapsMode(InputType.TYPE_TEXT_FLAG_CAP_WORDS));
        setSelection(6, 6);
        waitAndVerifyUpdateSelection(1, 6, 6, -1, -1);
        assertEquals(InputType.TYPE_TEXT_FLAG_CAP_WORDS,
                getCursorCapsMode(InputType.TYPE_TEXT_FLAG_CAP_WORDS));
        commitText("\n", 1);
        assertEquals(InputType.TYPE_TEXT_FLAG_CAP_WORDS,
                getCursorCapsMode(InputType.TYPE_TEXT_FLAG_CAP_WORDS));
    }

    private void clearEventLogs() throws Exception {
        final String code = "clearEventLogs()";
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getContentViewCore().getWebContents(), code);
    }

    private void waitForEventLogs(String expectedLogs) throws Exception {
        final String code = "getEventLogs()";
        final String sanitizedExpectedLogs = "\"" + expectedLogs + "\"";
        assertEquals(sanitizedExpectedLogs, JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                                    getContentViewCore().getWebContents(), code));
    }

    // https://crbug.com/604675
    @MediumTest
    @Feature({"TextInput"})
    public void testAlertInKeyUpListenerDoesNotCrash() throws Exception {
        // Call 'alert()' when 'keyup' event occurs. Since we are in contentshell,
        // this does not actually pops up the alert window.
        String code = "(function() { "
                + "var editor = document.getElementById('input_text');"
                + "editor.addEventListener('keyup', function(e) { alert('keyup') });"
                + "})();";
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getContentViewCore().getWebContents(), code);
        setComposingText("ab", 1);
        finishComposingText();
        assertEquals("ab", getTextBeforeCursor(10, 0));
    }

    // https://crbug.com/616334
    @SmallTest
    @Feature({"TextInput"})
    public void testCastToBaseInputConnection() throws Exception {
        commitText("a", 1);
        final BaseInputConnection baseInputConnection = (BaseInputConnection) mConnection;
        assertEquals("a", runBlockingOnImeThread(new Callable<CharSequence>() {
            @Override
            public CharSequence call() {
                return baseInputConnection.getTextBeforeCursor(10, 0);
            }
        }));
    }

    // Tests that the method call order is kept.
    // See crbug.com/601707 for details.
    @MediumTest
    @Feature({"TextInput"})
    public void testSetSelectionCommitTextOrder() throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        runBlockingOnImeThread(new Callable<Void>() {
            @Override
            public Void call() {
                connection.beginBatchEdit();
                connection.commitText("hello world", 1);
                connection.setSelection(6, 6);
                connection.deleteSurroundingText(0, 5);
                connection.commitText("'", 1);
                connection.commitText("world", 1);
                connection.setSelection(7, 7);
                connection.setComposingText("", 1);
                connection.endBatchEdit();
                return null;
            }
        });
        waitAndVerifyUpdateSelection(0, 7, 7, -1, -1);
    }

    // crbug.com/643477
    @MediumTest
    @Feature({"TextInput"})
    public void testUiThreadAccess() throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // We allow UI thread access for most functions, except for
                // beginBatchEdit(), endBatchEdit(), and get* methods().
                assertTrue(connection.commitText("a", 1));
                assertTrue(connection.setComposingText("b", 1));
                assertTrue(connection.setComposingText("bc", 1));
                assertTrue(connection.finishComposingText());
            }
        });
        assertEquals("abc", runBlockingOnImeThread(new Callable<CharSequence>() {
            @Override
            public CharSequence call() throws Exception {
                return connection.getTextBeforeCursor(5, 0);
            }
        }));
    }

    private void performGo(TestCallbackHelperContainer testCallbackHelperContainer)
            throws Throwable {
        final InputConnection inputConnection = mConnection;
        final Callable<Void> callable = new Callable<Void>() {
            @Override
            public Void call() throws Exception {
                inputConnection.performEditorAction(EditorInfo.IME_ACTION_GO);
                return null;
            }
        };

        handleBlockingCallbackAction(
                testCallbackHelperContainer.getOnPageFinishedHelper(),
                new Runnable() {
                    @Override
                    public void run() {
                        try {
                            runBlockingOnImeThread(callable);
                        } catch (Exception e) {
                            e.printStackTrace();
                            fail();
                        }
                    }
                });
    }

    private void assertWaitForKeyboardStatus(final boolean show) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                // We do not check the other way around: in some cases we need to keep
                // input connection even when the last known status is 'hidden'.
                if (show && getInputConnection() == null) {
                    updateFailureReason("input connection should not be null.");
                    return false;
                }
                updateFailureReason("expected show: " + show);
                return show == mInputMethodManagerWrapper.isShowWithoutHideOutstanding();
            }
        });
    }

    private void assertWaitForSelectActionBarStatus(final boolean show) {
        CriteriaHelper.pollUiThread(Criteria.equals(show, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mContentViewCore.isSelectActionBarShowing();
            }
        }));
    }

    private void waitAndVerifyUpdateSelection(final int index, final int selectionStart,
            final int selectionEnd, final int compositionStart, final int compositionEnd) {
        final List<Pair<Range, Range>> states = mInputMethodManagerWrapper.getUpdateSelectionList();
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return states.size() > index;
            }
        });
        Pair<Range, Range> selection = states.get(index);
        assertEquals(selectionStart, selection.first.start());
        assertEquals(selectionEnd, selection.first.end());
        assertEquals(compositionStart, selection.second.start());
        assertEquals(compositionEnd, selection.second.end());
    }

    private void resetUpdateSelectionList() {
        mInputMethodManagerWrapper.getUpdateSelectionList().clear();
    }

    private void assertClipboardContents(final Activity activity, final String expectedContents) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ClipboardManager clipboardManager =
                        (ClipboardManager) activity.getSystemService(Context.CLIPBOARD_SERVICE);
                ClipData clip = clipboardManager.getPrimaryClip();
                return clip != null && clip.getItemCount() == 1
                        && TextUtils.equals(clip.getItemAt(0).getText(), expectedContents);
            }
        });
    }

    private ImeAdapter getImeAdapter() {
        return mContentViewCore.getImeAdapterForTest();
    }

    private ChromiumBaseInputConnection getInputConnection() {
        try {
            return ThreadUtils.runOnUiThreadBlocking(new Callable<ChromiumBaseInputConnection>() {
                @Override
                public ChromiumBaseInputConnection call() {
                    return mContentViewCore.getImeAdapterForTest().getInputConnectionForTest();
                }
            });
        } catch (ExecutionException e) {
            e.printStackTrace();
            fail();
            return null;
        }
    }

    private void restartInput() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mImeAdapter.restartInput();
            }
        });
    }

    // After calling this method, we should call assertClipboardContents() to wait for the clipboard
    // to get updated. See cubug.com/621046
    private void copy() {
        final WebContents webContents = mWebContents;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                webContents.copy();
            }
        });
    }

    private void cut() {
        final WebContents webContents = mWebContents;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                webContents.cut();
            }
        });
    }

    private void setClip(final CharSequence text) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                final ClipboardManager clipboardManager =
                        (ClipboardManager) getActivity().getSystemService(
                                Context.CLIPBOARD_SERVICE);
                clipboardManager.setPrimaryClip(ClipData.newPlainText(null, text));
            }
        });
    }

    private void paste() {
        final WebContents webContents = mWebContents;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                webContents.paste();
            }
        });
    }

    private void selectAll() {
        final WebContents webContents = mWebContents;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                webContents.selectAll();
            }
        });
    }

    private void collapseSelection() {
        final WebContents webContents = mWebContents;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                webContents.collapseSelection();
            }
        });
    }

    /**
     * Run the {@Callable} on IME thread (or UI thread if not applicable).
     * @param c The callable
     * @return The result from running the callable.
     */
    protected <T> T runBlockingOnImeThread(Callable<T> c) throws Exception {
        return ImeTestUtils.runBlockingOnHandler(mConnectionFactory.getHandler(), c);
    }

    private boolean beginBatchEdit() throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return connection.beginBatchEdit();
            }
        });
    }

    private boolean endBatchEdit() throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return connection.endBatchEdit();
            }
        });
    }

    private boolean commitText(final CharSequence text, final int newCursorPosition)
            throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return connection.commitText(text, newCursorPosition);
            }
        });
    }

    private boolean setSelection(final int start, final int end) throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return connection.setSelection(start, end);
            }
        });
    }

    private boolean setComposingRegion(final int start, final int end) throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return connection.setComposingRegion(start, end);
            }
        });
    }

    protected boolean setComposingText(final CharSequence text, final int newCursorPosition)
            throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return connection.setComposingText(text, newCursorPosition);
            }
        });
    }

    private boolean finishComposingText() throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return connection.finishComposingText();
            }
        });
    }

    private boolean deleteSurroundingText(final int before, final int after) throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return connection.deleteSurroundingText(before, after);
            }
        });
    }

    // Note that deleteSurroundingTextInCodePoints() was introduced in Android N (Api level 24), but
    // the Android repository used in Chrome is behind that (level 23). So this function can't be
    // called by keyboard apps currently.
    @TargetApi(24)
    private boolean deleteSurroundingTextInCodePoints(final int before, final int after)
            throws Exception {
        final ThreadedInputConnection connection = (ThreadedInputConnection) mConnection;
        return runBlockingOnImeThread(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return connection.deleteSurroundingTextInCodePoints(before, after);
            }
        });
    }

    private CharSequence getTextBeforeCursor(final int length, final int flags) throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(new Callable<CharSequence>() {
            @Override
            public CharSequence call() {
                return connection.getTextBeforeCursor(length, flags);
            }
        });
    }

    private CharSequence getSelectedText(final int flags) throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(new Callable<CharSequence>() {
            @Override
            public CharSequence call() {
                return connection.getSelectedText(flags);
            }
        });
    }

    private CharSequence getTextAfterCursor(final int length, final int flags) throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(new Callable<CharSequence>() {
            @Override
            public CharSequence call() {
                return connection.getTextAfterCursor(length, flags);
            }
        });
    }

    private int getCursorCapsMode(final int reqModes) throws Throwable {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(new Callable<Integer>() {
            @Override
            public Integer call() {
                return connection.getCursorCapsMode(reqModes);
            }
        });
    }

    private void dispatchKeyEvent(final KeyEvent event) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mImeAdapter.dispatchKeyEvent(event);
            }
        });
    }

    /**
     * Focus element, wait for a single state update, reset state update list.
     * @param id ID of the element to focus.
     */
    private void focusElementAndWaitForStateUpdate(String id)
            throws InterruptedException, TimeoutException {
        resetUpdateSelectionList();
        focusElement(id);
        waitAndVerifyUpdateSelection(0, 0, 0, -1, -1);
        resetUpdateSelectionList();
    }

    private void focusElement(final String id) throws InterruptedException, TimeoutException {
        focusElement(id, true);
    }

    private void focusElement(final String id, boolean shouldShowKeyboard)
            throws InterruptedException, TimeoutException {
        DOMUtils.focusNode(mWebContents, id);
        assertWaitForKeyboardStatus(shouldShowKeyboard);
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(id, new Callable<String>() {
            @Override
            public String call() throws Exception {
                return DOMUtils.getFocusedNode(mWebContents);
            }
        }));
        // When we focus another element, the connection may be recreated.
        mConnection = getInputConnection();
    }

    private static class TestInputConnectionFactory implements ChromiumBaseInputConnection.Factory {
        private final ChromiumBaseInputConnection.Factory mFactory;

        private final List<Integer> mTextInputTypeList = new ArrayList<>();
        private EditorInfo mOutAttrs;

        public TestInputConnectionFactory(ChromiumBaseInputConnection.Factory factory) {
            mFactory = factory;
        }

        @Override
        public ChromiumBaseInputConnection initializeAndGet(View view, ImeAdapter imeAdapter,
                int inputType, int inputFlags, int inputMode, int selectionStart, int selectionEnd,
                EditorInfo outAttrs) {
            mTextInputTypeList.add(inputType);
            mOutAttrs = outAttrs;
            return mFactory.initializeAndGet(view, imeAdapter, inputType, inputMode, inputFlags,
                    selectionStart, selectionEnd, outAttrs);
        }

        @Override
        public Handler getHandler() {
            return mFactory.getHandler();
        }

        public Integer[] getTextInputTypeHistory() {
            Integer[] result = new Integer[mTextInputTypeList.size()];
            mTextInputTypeList.toArray(result);
            return result;
        }

        public void clearTextInputTypeHistory() {
            mTextInputTypeList.clear();
        }

        public EditorInfo getOutAttrs() {
            return mOutAttrs;
        }

        @Override
        public void onWindowFocusChanged(boolean gainFocus) {
            mFactory.onWindowFocusChanged(gainFocus);
        }

        @Override
        public void onViewFocusChanged(boolean gainFocus) {
            mFactory.onViewFocusChanged(gainFocus);
        }

        @Override
        public void onViewAttachedToWindow() {
            mFactory.onViewAttachedToWindow();
        }

        @Override
        public void onViewDetachedFromWindow() {
            mFactory.onViewDetachedFromWindow();
        }
    }
}
