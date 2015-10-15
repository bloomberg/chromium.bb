// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.Editable;
import android.text.Selection;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content.browser.test.util.TestInputMethodManagerWrapper.Range;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for text input using cases based on fixed regressions.
 */
public class ImeTest extends ContentShellTestBase {

    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\""
            + "content=\"width=device-width, initial-scale=2.0, maximum-scale=2.0\" /></head>"
            + "<body><form action=\"about:blank\">"
            + "<input id=\"input_text\" type=\"text\" /><br/>"
            + "<input id=\"input_radio\" type=\"radio\" style=\"width:50px;height:50px\" />"
            + "<br/><textarea id=\"textarea\" rows=\"4\" cols=\"20\"></textarea>"
            + "<br/><textarea id=\"textarea2\" rows=\"4\" cols=\"20\" autocomplete=\"off\">"
            + "</textarea>"
            + "<br/><p><span id=\"plain_text\">This is Plain Text One</span></p>"
            + "</form></body></html>");

    private TestAdapterInputConnection mConnection;
    private TestAdapterInputConnectionFactory mConnectionFactory;
    private ImeAdapter mImeAdapter;

    private ContentViewCore mContentViewCore;
    private WebContents mWebContents;
    private TestCallbackHelperContainer mCallbackContainer;
    private TestInputMethodManagerWrapper mInputMethodManagerWrapper;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        launchContentShellWithUrl(DATA_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
        mContentViewCore = getContentViewCore();
        mWebContents = getWebContents();

        mInputMethodManagerWrapper = new TestInputMethodManagerWrapper(mContentViewCore);
        getImeAdapter().setInputMethodManagerWrapper(mInputMethodManagerWrapper);
        assertEquals(0, mInputMethodManagerWrapper.getShowSoftInputCounter());
        mConnectionFactory = new TestAdapterInputConnectionFactory();
        mContentViewCore.setAdapterInputConnectionFactory(mConnectionFactory);

        mCallbackContainer = new TestCallbackHelperContainer(mContentViewCore);
        // TODO(aurimas) remove this wait once crbug.com/179511 is fixed.
        assertWaitForPageScaleFactorMatch(2);
        assertTrue(DOMUtils.waitForNonZeroNodeBounds(
                mWebContents, "input_text"));
        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);

        mConnection = (TestAdapterInputConnection) getAdapterInputConnection();
        mImeAdapter = getImeAdapter();

        // Two state updates from focus change and GestureTap.
        waitAndVerifyStatesAndCalls(0, "", 0, 0, -1, -1);
        waitAndVerifyStatesAndCalls(1, "", 0, 0, -1, -1);

        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());
        assertEquals(0, mInputMethodManagerWrapper.getEditorInfo().initialSelStart);
        assertEquals(0, mInputMethodManagerWrapper.getEditorInfo().initialSelEnd);

        resetUpdateStateList();
    }

    private void assertNoFurtherStateUpdate(final int index) throws InterruptedException {
        final List<TestImeState> states = mConnectionFactory.getImeStateList();
        assertFalse(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return states.size() > index;
            }
        }));
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testSetUpGeneratesNoFurtherStateUpdate() throws Throwable {
        assertNoFurtherStateUpdate(0);
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testKeyboardDismissedAfterClickingGo() throws Throwable {
        setComposingText("hello", 1);
        waitAndVerifyStatesAndCalls(0, "hello", 5, 5, 0, 5);

        performGo(mCallbackContainer);

        waitAndVerifyStatesAndCalls(1, "", 0, 0, -1, -1);
        assertWaitForKeyboardStatus(false);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    @RerunWithUpdatedContainerView
    public void testGetTextUpdatesAfterEnteringText() throws Throwable {
        setComposingText("h", 1);
        waitAndVerifyStates(0, "h", 1, 1, 0, 1);
        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());

        setComposingText("he", 1);
        waitAndVerifyStates(1, "he", 2, 2, 0, 2);
        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());

        setComposingText("hel", 1);
        waitAndVerifyStates(2, "hel", 3, 3, 0, 3);
        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());

        commitText("hel", 1);
        waitAndVerifyStates(3, "hel", 3, 3, -1, -1);
        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());
    }

    @SmallTest
    @Feature({"TextInput"})
    @RerunWithUpdatedContainerView
    public void testImeCopy() throws Exception {
        commitText("hello", 1);
        waitAndVerifyStates(0, "hello", 5, 5, -1, -1);

        setSelection(2, 5);
        waitAndVerifyStates(1, "hello", 2, 5, -1, -1);

        copy();
        assertClipboardContents(getActivity(), "llo");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testEnterTextAndRefocus() throws Exception {
        commitText("hello", 1);
        waitAndVerifyStatesAndCalls(0, "hello", 5, 5, -1, -1);

        DOMUtils.clickNode(this, mContentViewCore, "input_radio");
        assertWaitForKeyboardStatus(false);

        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        assertEquals(5, mInputMethodManagerWrapper.getEditorInfo().initialSelStart);
        assertEquals(5, mInputMethodManagerWrapper.getEditorInfo().initialSelEnd);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testKeyboardNotDismissedAfterCopySelection() throws Exception {
        commitText("Sample Text", 1);
        waitAndVerifyStatesAndCalls(0, "Sample Text", 11, 11, -1, -1);
        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        selectAll();
        copy();
        assertWaitForKeyboardStatus(true);
        assertEquals(11, Selection.getSelectionEnd(mContentViewCore.getEditableForTest()));
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeNotDismissedAfterCutSelection() throws Exception {
        commitText("Sample Text", 1);
        waitAndVerifyStatesAndCalls(0, "Sample Text", 11, 11, -1, -1);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
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
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(false);
        commitText("Sample Text", 1);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarShownOnLongPressingInput() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(false);
        commitText("Sample Text", 1);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(true);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testLongPressInputWhileComposingText() throws Exception {
        assertWaitForSelectActionBarStatus(false);
        setComposingText("Sample Text", 1);
        waitAndVerifyStatesAndCalls(0, "Sample Text", 11, 11, 0, 11);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");

        assertWaitForSelectActionBarStatus(true);

        // Long press will first change selection region, and then trigger IME app to show up.
        // See RenderFrameImpl::didChangeSelection() and RenderWidget::didHandleGestureEvent().
        waitAndVerifyStatesAndCalls(1, "Sample Text", 7, 11, 0, 11);
        waitAndVerifyStatesAndCalls(2, "Sample Text", 7, 11, 0, 11);

        // Now IME app wants to finish composing text because an external selection
        // change has been detected. At least Google Latin IME and Samsung IME
        // behave this way.
        finishComposingText();
        waitAndVerifyStatesAndCalls(3, "Sample Text", 7, 11, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeShownWhenLongPressOnAlreadySelectedText() throws Exception {
        assertWaitForSelectActionBarStatus(false);
        commitText("Sample Text", 1);

        int showCount = mInputMethodManagerWrapper.getShowSoftInputCounter();
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(true);
        assertEquals(showCount + 1, mInputMethodManagerWrapper.getShowSoftInputCounter());

        // Now long press again. Selection region remains the same, but the logic
        // should trigger IME to show up. Note that Android does not provide show /
        // hide status of IME, so we will just check whether showIme() has been triggered.
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        final int newCount = showCount + 2;
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return newCount == mInputMethodManagerWrapper.getShowSoftInputCounter();
            }
        }));
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarClearedOnTappingInput() throws Exception {
        commitText("Sample Text", 1);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        assertWaitForSelectActionBarStatus(true);
        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarClearedOnTappingOutsideInput() throws Exception {
        commitText("Sample Text", 1);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        assertWaitForSelectActionBarStatus(true);
        DOMUtils.clickNode(this, mContentViewCore, "input_radio");
        assertWaitForKeyboardStatus(false);
        assertWaitForSelectActionBarStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeNotShownOnLongPressingDifferentEmptyInputs() throws Exception {
        DOMUtils.focusNode(mWebContents, "input_radio");
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(false);
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForKeyboardStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeStaysOnLongPressingDifferentNonEmptyInputs() throws Exception {
        DOMUtils.focusNode(mWebContents, "input_text");
        assertWaitForKeyboardStatus(true);
        commitText("Sample Text", 1);
        DOMUtils.focusNode(mWebContents, "textarea");
        commitText("Sample Text", 1);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForKeyboardStatus(true);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeCut() throws Exception {
        commitText("snarful", 1);
        waitAndVerifyStatesAndCalls(0, "snarful", 7, 7, -1, -1);

        setSelection(1, 5);
        waitAndVerifyStatesAndCalls(1, "snarful", 1, 5, -1, -1);

        cut();
        waitAndVerifyStatesAndCalls(2, "sul", 1, 1, -1, -1);

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
        waitAndVerifyStatesAndCalls(0, "blarg", 5, 5, -1, -1);

        setSelection(3, 5);
        waitAndVerifyStatesAndCalls(1, "blarg", 3, 5, -1, -1);

        paste();
        // Paste is a two step process when there is a non-zero selection.
        waitAndVerifyStates(2, "bla", 3, 3, -1, -1);
        waitAndVerifyStatesAndCalls(3, "blablarg", 8, 8, -1, -1);

        paste();
        waitAndVerifyStatesAndCalls(4, "blablargblarg", 13, 13, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeSelectAndUnSelectAll() throws Exception {
        commitText("hello", 1);
        waitAndVerifyStatesAndCalls(0, "hello", 5, 5, -1, -1);

        selectAll();
        waitAndVerifyStatesAndCalls(1, "hello", 0, 5, -1, -1);

        unselect();
        waitAndVerifyStatesAndCalls(2, "", 0, 0, -1, -1);

        assertWaitForKeyboardStatus(false);
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
        waitAndVerifyStatesAndCalls(0, "hllo", 4, 4, -1, -1);

        commitText(" ", 1);
        waitAndVerifyStatesAndCalls(1, "hllo ", 5, 5, -1, -1);

        setSelection(1, 1);
        waitAndVerifyStatesAndCalls(2, "hllo ", 1, 1, -1, -1);

        setComposingRegion(0, 4);
        waitAndVerifyStatesAndCalls(3, "hllo ", 1, 1, 0, 4);

        finishComposingText();
        waitAndVerifyStatesAndCalls(4, "hllo ", 1, 1, -1, -1);

        commitText("\n", 1);
        waitAndVerifyStatesAndCalls(5, "h\nllo ", 2, 2, -1, -1);
    }

    /*
    @SmallTest
    @Feature({"TextInput", "Main"})
    http://crbug.com/445499
    */
    public void testDeleteText() throws Throwable {
        focusElement("textarea");

        // The calls below are a reflection of what the stock Google Keyboard (Andr
        // when the noted key is touched on screen.
        // H
        resetUpdateStateList();
        setComposingText("h", 1);
        assertUpdateStateCall(1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // O
        resetUpdateStateList();
        setComposingText("ho", 1);
        assertUpdateStateCall(1000);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));

        resetUpdateStateList();
        setComposingText("h", 1);
        assertUpdateStateCall(1000);
        setComposingRegion(0, 1);
        setComposingText("h", 1);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // I
        setComposingText("hi", 1);
        assertEquals("hi", mConnection.getTextBeforeCursor(9, 0));

        // SPACE
        commitText("hi", 1);
        commitText(" ", 1);
        assertEquals("hi ", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        deleteSurroundingText(1, 0);
        setComposingRegion(0, 2);
        assertEquals("hi", mConnection.getTextBeforeCursor(9, 0));

        setComposingText("h", 1);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        commitText("", 1);
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));

        // DEL (on empty input)
        deleteSurroundingText(1, 0); // DEL on empty still sends 1,0
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));
    }


    /*
    @SmallTest
    @Feature({"TextInput", "Main"})
    */
    public void testSwipingText() throws Throwable {
        focusElement("textarea");

        // The calls below are a reflection of what the stock Google Keyboard (Android 4.4) sends
        // when the word is swiped on the soft keyboard.  Exercise care when altering to make sure
        // that the test reflects reality.  If this test breaks, it's possible that code has
        // changed and different calls need to be made instead.
        // "three"
        resetUpdateStateList();
        setComposingText("three", 1);
        assertUpdateStateCall(1000);
        assertEquals("three", mConnection.getTextBeforeCursor(99, 0));

        // "word"
        commitText("three", 1);
        commitText(" ", 1);
        setComposingText("word", 1);
        resetUpdateStateList();
        assertUpdateStateCall(1000);
        assertEquals("three word", mConnection.getTextBeforeCursor(99, 0));

        // "test"
        commitText("word", 1);
        commitText(" ", 1);
        resetUpdateStateList();
        setComposingText("test", 1);
        assertUpdateStateCall(1000);
        assertEquals("three word test", mConnection.getTextBeforeCursor(99, 0));
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDeleteMultiCharacterCodepoint() throws Throwable {
        // This smiley is a multi character codepoint.
        final String smiley = "\uD83D\uDE0A";

        commitText(smiley, 1);
        waitAndVerifyStatesAndCalls(0, smiley, 2, 2, -1, -1);

        // DEL, sent via dispatchKeyEvent like it is in Android WebView or a physical keyboard.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));

        waitAndVerifyStatesAndCalls(1, "", 0, 0, -1, -1);

        // Make sure that we accept further typing after deleting the smiley.
        setComposingText("s", 1);
        waitAndVerifyStatesAndCalls(2, "s", 1, 1, 0, 1);
        setComposingText("sm", 1);
        waitAndVerifyStatesAndCalls(3, "sm", 2, 2, 0, 2);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testBackspaceKeycode() throws Throwable {
        focusElement("textarea");

        // H
        resetUpdateStateList();
        commitText("h", 1);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // O
        resetUpdateStateList();
        commitText("o", 1);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(1000);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));

        // DEL, sent via dispatchKeyEvent like it is in Android WebView or a physical keyboard.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));

        // DEL
        resetUpdateStateList();
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testRepeatBackspaceKeycode() throws Throwable {
        focusElement("textarea");

        // H
        resetUpdateStateList();
        commitText("h", 1);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // O
        resetUpdateStateList();
        commitText("o", 1);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(1000);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));

        // Multiple keydowns should each delete one character (this is for physical keyboard
        // key-repeat).
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));

        // DEL
        resetUpdateStateList();
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(1000);
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testPhysicalKeyboard() throws Throwable {
        focusElementAndWaitForStateUpdate("textarea");

        // Type 'a' using a physical keyboard.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_A));
        waitAndVerifyStatesAndCalls(0, "a", 1, 1, -1, -1);

        // Type 'enter' key.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
        waitAndVerifyStatesAndCalls(1, "a\n\n", 2, 2, -1, -1);

        // Type 'b'.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_B));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_B));
        waitAndVerifyStatesAndCalls(2, "a\nb", 3, 3, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testPhysicalKeyboard_AccentKeyCodes() throws Throwable {
        focusElementAndWaitForStateUpdate("textarea");

        // h
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_H));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_H));
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
        waitAndVerifyStatesAndCalls(0, "h", 1, 1, -1, -1);

        // ALT-i  (circumflex accent key on virtual keyboard)
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertUpdateStateCall(1000);
        assertEquals("hˆ", mConnection.getTextBeforeCursor(9, 0));
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertEquals("hˆ", mConnection.getTextBeforeCursor(9, 0));
        waitAndVerifyStatesAndCalls(1, "hˆ", 2, 2, 1, 2);

        // o
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_O));
        assertUpdateStateCall(1000);
        assertEquals("hô", mConnection.getTextBeforeCursor(9, 0));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_O));
        assertEquals("hô", mConnection.getTextBeforeCursor(9, 0));
        waitAndVerifyStatesAndCalls(2, "hô", 2, 2, -1, -1);

        // ALT-i
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertUpdateStateCall(1000);
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertEquals("hôˆ", mConnection.getTextBeforeCursor(9, 0));
        waitAndVerifyStatesAndCalls(3, "hôˆ", 3, 3, 2, 3);

        // ALT-i again should have no effect
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertUpdateStateCall(1000);
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertEquals("hôˆ", mConnection.getTextBeforeCursor(9, 0));

        // b (cannot be accented, should just appear after)
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_B));
        assertUpdateStateCall(1000);
        assertEquals("hôˆb", mConnection.getTextBeforeCursor(9, 0));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_B));
        assertEquals("hôˆb", mConnection.getTextBeforeCursor(9, 0));
        // A transitional state due to finishComposingText.
        waitAndVerifyStates(4, "hôˆ", 3, 3, -1, -1);
        waitAndVerifyStatesAndCalls(5, "hôˆb", 4, 4, -1, -1);

        // ALT-i
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertUpdateStateCall(1000);
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertEquals("hôˆbˆ", mConnection.getTextBeforeCursor(9, 0));
        waitAndVerifyStatesAndCalls(6, "hôˆbˆ", 5, 5, 4, 5);

        // Backspace
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        assertUpdateStateCall(1000);
        assertEquals("hôˆb", mConnection.getTextBeforeCursor(9, 0));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));
        assertEquals("hôˆb", mConnection.getTextBeforeCursor(9, 0));
        // A transitional state due to finishComposingText in deleteSurroundingTextImpl.
        waitAndVerifyStates(7, "hôˆbˆ", 5, 5, -1, -1);
        waitAndVerifyStatesAndCalls(8, "hôˆb", 4, 4, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testSetComposingRegionOutOfBounds() throws Throwable {
        focusElement("textarea");
        setComposingText("hello", 1);

        setComposingRegion(0, 0);
        setComposingRegion(0, 9);
        setComposingRegion(9, 0);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testEnterKey_AfterCommitText() throws Throwable {
        focusElementAndWaitForStateUpdate("textarea");

        commitText("hello", 1);
        waitAndVerifyStatesAndCalls(0, "hello", 5, 5, -1, -1);

        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
        // TODO(aurimas): remove this workaround when crbug.com/278584 is fixed.
        // The second new line is not a user visible/editable one, it is a side-effect of Blink
        // using <br> internally. This only happens when \n is at the end.
        waitAndVerifyStatesAndCalls(1, "hello\n\n", 6, 6, -1, -1);

        commitText("world", 1);
        waitAndVerifyStatesAndCalls(2, "hello\nworld", 11, 11, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testEnterKey_WhileComposingText() throws Throwable {
        focusElementAndWaitForStateUpdate("textarea");

        setComposingText("hello", 1);
        waitAndVerifyStatesAndCalls(0, "hello", 5, 5, 0, 5);

        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));

        // TODO(aurimas): remove this workaround when crbug.com/278584 is fixed.
        // A transitional state due to finishComposingText.
        waitAndVerifyStates(1, "hello", 5, 5, -1, -1);
        // The second new line is not a user visible/editable one, it is a side-effect of Blink
        // using <br> internally. This only happens when \n is at the end.
        waitAndVerifyStatesAndCalls(2, "hello\n\n", 6, 6, -1, -1);

        commitText("world", 1);
        waitAndVerifyStatesAndCalls(3, "hello\nworld", 11, 11, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDpadKeyCodesWhileSwipingText() throws Throwable {
        focusElement("textarea");

        // DPAD_CENTER should cause keyboard to appear
        resetUpdateStateList();
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_CENTER));
        assertUpdateStateCall(1000);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupShowAndHide() throws Throwable {
        commitText("hello", 1);
        waitAndVerifyStatesAndCalls(0, "hello", 5, 5, -1, -1);

        selectAll();
        waitAndVerifyStatesAndCalls(1, "hello", 0, 5, -1, -1);

        cut();
        waitAndVerifyStatesAndCalls(2, "", 0, 0, -1, -1);

        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mContentViewCore.isPastePopupShowing();
            }
        }));

        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        setComposingText("h", 1);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !mContentViewCore.isPastePopupShowing();
            }
        }));
        assertFalse(mContentViewCore.hasInsertion());
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectionClearedOnKeyEvent() throws Throwable {
        commitText("hello", 1);
        waitAndVerifyStatesAndCalls(0, "hello", 5, 5, -1, -1);

        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);

        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(true);

        setComposingText("h", 1);
        assertWaitForSelectActionBarStatus(false);
        assertFalse(mContentViewCore.hasSelection());
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testTextHandlesPreservedWithDpadNavigation() throws Throwable {
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text");
        assertWaitForSelectActionBarStatus(true);
        assertTrue(mContentViewCore.hasSelection());

        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_DOWN));
        assertWaitForSelectActionBarStatus(true);
        assertTrue(mContentViewCore.hasSelection());
    }

    @MediumTest
    @Feature({"TextInput"})
    public void testRestartInputWhileComposingText() throws Throwable {
        setComposingText("abc", 1);
        waitAndVerifyStatesAndCalls(0, "abc", 3, 3, 0, 3);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mConnection.restartInput();
            }
        });
        // We don't do anything when input gets restarted. But we depend on Android's
        // InputMethodManager and/or input methods to call finishComposingText() in setting
        // current input connection as active or finishing the current input connection.
        assertNoFurtherStateUpdate(1);
    }

    private void performGo(TestCallbackHelperContainer testCallbackHelperContainer)
            throws Throwable {
        final AdapterInputConnection inputConnection = mConnection;
        handleBlockingCallbackAction(
                testCallbackHelperContainer.getOnPageFinishedHelper(),
                new Runnable() {
                    @Override
                    public void run() {
                        inputConnection.performEditorAction(EditorInfo.IME_ACTION_GO);
                    }
                });
    }

    private void assertWaitForKeyboardStatus(final boolean show) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return show == getImeAdapter().mIsShowWithoutHideOutstanding
                        && (!show || getAdapterInputConnection() != null);
            }
        }));
    }

    private void assertWaitForSelectActionBarStatus(
            final boolean show) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return show == mContentViewCore.isSelectActionBarShowing();
            }
        }));
    }

    private void waitAndVerifyStates(final int index, String text, final int selectionStart,
            final int selectionEnd, final int compositionStart, final int compositionEnd)
            throws InterruptedException {
        final List<TestImeState> states = mConnectionFactory.getImeStateList();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return states.size() > index;
            }
        }));
        states.get(index).assertEqualState(
                text, selectionStart, selectionEnd, compositionStart, compositionEnd);
    }

    private void waitAndVerifyStatesAndCalls(final int index, String text, final int selectionStart,
            final int selectionEnd, final int compositionStart, final int compositionEnd)
            throws InterruptedException {
        waitAndVerifyStates(
                index, text, selectionStart, selectionEnd, compositionStart, compositionEnd);

        // Wait and verify calls to InputMethodManager.
        final Range selection = new Range(selectionStart, selectionEnd);
        final Range composition = new Range(compositionStart, compositionEnd);
        assertTrue("Actual selection was: " + mInputMethodManagerWrapper.getSelection()
                        + ", and actual composition was: "
                        + mInputMethodManagerWrapper.getComposition(),
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mInputMethodManagerWrapper.getSelection().equals(selection)
                                && mInputMethodManagerWrapper.getComposition().equals(composition);
                    }
                }));
    }

    private void resetUpdateStateList() {
        mConnectionFactory.getImeStateList().clear();
    }

    private void assertUpdateStateCall(int maxms) throws Exception {
        while (mConnectionFactory.getImeStateList().size() == 0 && maxms > 0) {
            try {
                Thread.sleep(50);
            } catch (Exception e) {
                // Not really a problem since we're just going to sleep again.
            }
            maxms -= 50;
        }
        assertTrue(mConnectionFactory.getImeStateList().size() > 0);
    }

    private void assertClipboardContents(final Activity activity, final String expectedContents)
            throws InterruptedException {
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ClipboardManager clipboardManager =
                        (ClipboardManager) activity.getSystemService(Context.CLIPBOARD_SERVICE);
                ClipData clip = clipboardManager.getPrimaryClip();
                return clip != null && clip.getItemCount() == 1
                        && TextUtils.equals(clip.getItemAt(0).getText(), expectedContents);
            }
        }));
    }

    private ImeAdapter getImeAdapter() {
        return mContentViewCore.getImeAdapterForTest();
    }

    private AdapterInputConnection getAdapterInputConnection() {
        return mContentViewCore.getInputConnectionForTest();
    }

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

    private void unselect() {
        final WebContents webContents = mWebContents;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                webContents.unselect();
            }
        });
    }

    private void commitText(final CharSequence text, final int newCursorPosition) {
        final AdapterInputConnection connection = mConnection;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.commitText(text, newCursorPosition);
            }
        });
    }

    private void setSelection(final int start, final int end) {
        final AdapterInputConnection connection = mConnection;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.setSelection(start, end);
            }
        });
    }

    private void setComposingRegion(final int start, final int end) {
        final AdapterInputConnection connection = mConnection;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.setComposingRegion(start, end);
            }
        });
    }

    private void setComposingText(final CharSequence text, final int newCursorPosition) {
        final AdapterInputConnection connection = mConnection;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.setComposingText(text, newCursorPosition);
            }
        });
    }

    private void finishComposingText() {
        final AdapterInputConnection connection = mConnection;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.finishComposingText();
            }
        });
    }

    private void deleteSurroundingText(final int before, final int after) {
        final AdapterInputConnection connection = mConnection;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.deleteSurroundingText(before, after);
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
        focusElement(id);
        waitAndVerifyStatesAndCalls(0, "", 0, 0, -1, -1);
        resetUpdateStateList();
    }

    private void focusElement(final String id) throws InterruptedException, TimeoutException {
        DOMUtils.focusNode(mWebContents, id);
        assertWaitForKeyboardStatus(true);
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return id.equals(DOMUtils.getFocusedNode(mWebContents));
                } catch (Exception e) {
                    return false;
                }
            }
        }));
        // When we focus another element, the connection may be recreated.
        mConnection = (TestAdapterInputConnection) getAdapterInputConnection();
    }

    private static class TestAdapterInputConnectionFactory extends
            ImeAdapter.AdapterInputConnectionFactory {
        private final List<TestImeState> mImeStateList = new ArrayList<>();

        @Override
        public AdapterInputConnection get(View view, ImeAdapter imeAdapter,
                Editable editable, EditorInfo outAttrs) {
            return new TestAdapterInputConnection(
                    mImeStateList, view, imeAdapter, editable, outAttrs);
        }

        public List<TestImeState> getImeStateList() {
            return mImeStateList;
        }
    }

    private static class TestAdapterInputConnection extends AdapterInputConnection {
        private final List<TestImeState> mImeStateList;

        public TestAdapterInputConnection(List<TestImeState> imeStateList, View view,
                ImeAdapter imeAdapter, Editable editable, EditorInfo outAttrs) {
            super(view, imeAdapter, editable, outAttrs);
            mImeStateList = imeStateList;
        }

        @Override
        public void updateState(String text, int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd, boolean requiredAck) {
            mImeStateList.add(new TestImeState(
                    text, selectionStart, selectionEnd, compositionStart, compositionEnd));
            super.updateState(text, selectionStart, selectionEnd, compositionStart,
                    compositionEnd, requiredAck);
        }
    }

    private static class TestImeState {
        private final String mText;
        private final int mSelectionStart;
        private final int mSelectionEnd;
        private final int mCompositionStart;
        private final int mCompositionEnd;

        public TestImeState(String text, int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd) {
            mText = text;
            mSelectionStart = selectionStart;
            mSelectionEnd = selectionEnd;
            mCompositionStart = compositionStart;
            mCompositionEnd = compositionEnd;
        }

        public void assertEqualState(String text, int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd) {
            assertEquals("Text did not match", text, mText);
            assertEquals("Selection start did not match", selectionStart, mSelectionStart);
            assertEquals("Selection end did not match", selectionEnd, mSelectionEnd);
            assertEquals("Composition start did not match", compositionStart, mCompositionStart);
            assertEquals("Composition end did not match", compositionEnd, mCompositionEnd);
        }
    }
}
