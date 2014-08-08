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
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.ArrayList;
import java.util.concurrent.Callable;

/**
 * Integration tests for text input using cases based on fixed regressions.
 */
public class ImeTest extends ContentShellTestBase {

    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\"" +
            "content=\"width=device-width, initial-scale=2.0, maximum-scale=2.0\" /></head>" +
            "<body><form action=\"about:blank\">" +
            "<input id=\"input_text\" type=\"text\" /><br/>" +
            "<input id=\"input_radio\" type=\"radio\" style=\"width:50px;height:50px\" />" +
            "<br/><textarea id=\"textarea\" rows=\"4\" cols=\"20\"></textarea>" +
            "</form></body></html>");

    private TestAdapterInputConnection mConnection;
    private ImeAdapter mImeAdapter;

    private ContentViewCore mContentViewCore;
    private TestCallbackHelperContainer mCallbackContainer;
    private TestInputMethodManagerWrapper mInputMethodManagerWrapper;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        launchContentShellWithUrl(DATA_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
        mContentViewCore = getContentViewCore();

        mInputMethodManagerWrapper = new TestInputMethodManagerWrapper(mContentViewCore);
        getImeAdapter().setInputMethodManagerWrapper(mInputMethodManagerWrapper);
        assertEquals(0, mInputMethodManagerWrapper.getShowSoftInputCounter());
        mContentViewCore.setAdapterInputConnectionFactory(
                new TestAdapterInputConnectionFactory());

        mCallbackContainer = new TestCallbackHelperContainer(mContentViewCore);
        // TODO(aurimas) remove this wait once crbug.com/179511 is fixed.
        assertWaitForPageScaleFactorMatch(2);
        assertTrue(DOMUtils.waitForNonZeroNodeBounds(
                mContentViewCore, "input_text"));
        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);

        mConnection = (TestAdapterInputConnection) getAdapterInputConnection();
        mImeAdapter = getImeAdapter();

        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 0, "", 0, 0, -1, -1);
        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());
        assertEquals(0, mInputMethodManagerWrapper.getEditorInfo().initialSelStart);
        assertEquals(0, mInputMethodManagerWrapper.getEditorInfo().initialSelEnd);
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testKeyboardDismissedAfterClickingGo() throws Throwable {
        setComposingText(mConnection, "hello", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, 0, 5);

        performGo(getAdapterInputConnection(), mCallbackContainer);

        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "", 0, 0, -1, -1);
        assertWaitForKeyboardStatus(false);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    @RerunWithUpdatedContainerView
    public void testGetTextUpdatesAfterEnteringText() throws Throwable {
        setComposingText(mConnection, "h", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "h", 1, 1, 0, 1);
        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());

        setComposingText(mConnection, "he", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "he", 2, 2, 0, 2);
        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());

        setComposingText(mConnection, "hel", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 3, "hel", 3, 3, 0, 3);
        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());

        commitText(mConnection, "hel", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 4, "hel", 3, 3, -1, -1);
        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());
    }

    @SmallTest
    @Feature({"TextInput"})
    @RerunWithUpdatedContainerView
    public void testImeCopy() throws Exception {
        commitText(mConnection, "hello", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, -1, -1);

        setSelection(mConnection, 2, 5);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "hello", 2, 5, -1, -1);

        copy(mImeAdapter);
        assertClipboardContents(getActivity(), "llo");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testEnterTextAndRefocus() throws Exception {
        commitText(mConnection, "hello", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, -1, -1);

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
        commitText(mConnection, "Sample Text", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1,
                "Sample Text", 11, 11, -1, -1);
        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        selectAll(mImeAdapter);
        copy(mImeAdapter);
        assertWaitForKeyboardStatus(true);
        assertEquals(11, Selection.getSelectionEnd(mContentViewCore.getEditableForTest()));
        assertEquals(11, Selection.getSelectionEnd(mContentViewCore.getEditableForTest()));
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeCut() throws Exception {
        commitText(mConnection, "snarful", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "snarful", 7, 7, -1, -1);

        setSelection(mConnection, 1, 5);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "snarful", 1, 5, -1, -1);

        cut(mImeAdapter);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 3, "sul", 1, 1, -1, -1);

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

        paste(mImeAdapter);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "blarg", 5, 5, -1, -1);

        setSelection(mConnection, 3, 5);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "blarg", 3, 5, -1, -1);

        paste(mImeAdapter);
        // Paste is a two step process when there is a non-zero selection.
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 3, "bla", 3, 3, -1, -1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 4, "blablarg", 8, 8, -1, -1);

        paste(mImeAdapter);
        waitAndVerifyEditableCallback(
                mConnection.mImeUpdateQueue, 5, "blablargblarg", 13, 13, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeSelectAndUnSelectAll() throws Exception {
        commitText(mConnection, "hello", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, -1, -1);

        selectAll(mImeAdapter);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "hello", 0, 5, -1, -1);

        unselect(mImeAdapter);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 3, "", 0, 0, -1, -1);

        assertWaitForKeyboardStatus(false);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testShowImeIfNeeded() throws Throwable {
        // showImeIfNeeded() is now implicitly called by the updated focus
        // heuristic so no need to call explicitly. http://crbug.com/371927
        DOMUtils.focusNode(mContentViewCore, "input_radio");
        assertWaitForKeyboardStatus(false);

        DOMUtils.focusNode(mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testFinishComposingText() throws Throwable {
        DOMUtils.focusNode(mContentViewCore, "input_radio");
        assertWaitForKeyboardStatus(false);
        DOMUtils.focusNode(mContentViewCore, "textarea");
        assertWaitForKeyboardStatus(true);

        mConnection = (TestAdapterInputConnection) getAdapterInputConnection();
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 0, "", 0, 0, -1, -1);

        commitText(mConnection, "hllo", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "hllo", 4, 4, -1, -1);

        commitText(mConnection, " ", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "hllo ", 5, 5, -1, -1);

        setSelection(mConnection, 1, 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 3, "hllo ", 1, 1, -1, -1);

        setComposingRegion(mConnection, 0, 4);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 4, "hllo ", 1, 1, 0, 4);

        finishComposingText(mConnection);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 5, "hllo ", 1, 1, -1, -1);

        commitText(mConnection, "\n", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 6, "h\nllo ", 2, 2, -1, -1);
    }

    private int getTypedKeycodeGuess(String before, String after) {
        KeyEvent ev = ImeAdapter.getTypedKeyEventGuess(before, after);
        if (ev == null) return -1;
        return ev.getKeyCode();
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testGuessedKeyCodeFromTyping() throws Throwable {
        assertEquals(-1, getTypedKeycodeGuess(null, ""));
        assertEquals(KeyEvent.KEYCODE_X, getTypedKeycodeGuess(null, "x"));
        assertEquals(-1, getTypedKeycodeGuess(null, "xyz"));

        assertEquals(-1, getTypedKeycodeGuess("abc", "abc"));
        assertEquals(KeyEvent.KEYCODE_DEL, getTypedKeycodeGuess("abc", ""));

        assertEquals(KeyEvent.KEYCODE_H, getTypedKeycodeGuess("", "h"));
        assertEquals(KeyEvent.KEYCODE_DEL, getTypedKeycodeGuess("h", ""));
        assertEquals(KeyEvent.KEYCODE_E, getTypedKeycodeGuess("h", "he"));
        assertEquals(KeyEvent.KEYCODE_L, getTypedKeycodeGuess("he", "hel"));
        assertEquals(KeyEvent.KEYCODE_O, getTypedKeycodeGuess("hel", "helo"));
        assertEquals(KeyEvent.KEYCODE_DEL, getTypedKeycodeGuess("helo", "hel"));
        assertEquals(KeyEvent.KEYCODE_L, getTypedKeycodeGuess("hel", "hell"));
        assertEquals(KeyEvent.KEYCODE_L, getTypedKeycodeGuess("hell", "helll"));
        assertEquals(KeyEvent.KEYCODE_DEL, getTypedKeycodeGuess("helll", "hell"));
        assertEquals(KeyEvent.KEYCODE_O, getTypedKeycodeGuess("hell", "hello"));

        assertEquals(KeyEvent.KEYCODE_X, getTypedKeycodeGuess("xxx", "xxxx"));
        assertEquals(KeyEvent.KEYCODE_X, getTypedKeycodeGuess("xxx", "xxxxx"));
        assertEquals(KeyEvent.KEYCODE_DEL, getTypedKeycodeGuess("xxx", "xx"));
        assertEquals(KeyEvent.KEYCODE_DEL, getTypedKeycodeGuess("xxx", "x"));

        assertEquals(KeyEvent.KEYCODE_Y, getTypedKeycodeGuess("xxx", "xxxy"));
        assertEquals(KeyEvent.KEYCODE_Y, getTypedKeycodeGuess("xxx", "xxxxy"));
        assertEquals(-1, getTypedKeycodeGuess("xxx", "xy"));
        assertEquals(-1, getTypedKeycodeGuess("xxx", "y"));

        assertEquals(-1, getTypedKeycodeGuess("foo", "bar"));
        assertEquals(-1, getTypedKeycodeGuess("foo", "bars"));
        assertEquals(-1, getTypedKeycodeGuess("foo", "ba"));

        // Some characters also require modifiers so we have to check the full event.
        KeyEvent ev = ImeAdapter.getTypedKeyEventGuess(null, "!");
        assertEquals(KeyEvent.KEYCODE_1, ev.getKeyCode());
        assertTrue(ev.isShiftPressed());
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testKeyCodesWhileComposingText() throws Throwable {
        DOMUtils.focusNode(mContentViewCore, "textarea");
        assertWaitForKeyboardStatus(true);

        // The calls below are a reflection of what the stock Google Keyboard (Android 4.4) sends
        // when the noted key is touched on screen.  Exercise care when altering to make sure
        // that the test reflects reality.  If this test breaks, it's possible that code has
        // changed and different calls need to be made instead.
        mConnection = (TestAdapterInputConnection) getAdapterInputConnection();
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 0, "", 0, 0, -1, -1);

        // H
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "h", 1);
        assertEquals(KeyEvent.KEYCODE_H, mImeAdapter.mLastSyntheticKeyCode);
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // O
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "ho", 1);
        assertEquals(KeyEvent.KEYCODE_O, mImeAdapter.mLastSyntheticKeyCode);
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "h", 1);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertUpdateStateCall(mConnection, 1000);
        setComposingRegion(mConnection, 0, 1);  // DEL calls cancelComposition() then restarts
        setComposingText(mConnection, "h", 1);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // I
        setComposingText(mConnection, "hi", 1);
        assertEquals(KeyEvent.KEYCODE_I, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("hi", mConnection.getTextBeforeCursor(9, 0));

        // SPACE
        commitText(mConnection, "hi", 1);
        assertEquals(-1, mImeAdapter.mLastSyntheticKeyCode);
        commitText(mConnection, " ", 1);
        assertEquals(KeyEvent.KEYCODE_SPACE, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("hi ", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        deleteSurroundingText(mConnection, 1, 0);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        setComposingRegion(mConnection, 0, 2);
        assertEquals("hi", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        setComposingText(mConnection, "h", 1);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        commitText(mConnection, "", 1);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));

        // DEL (on empty input)
        deleteSurroundingText(mConnection, 1, 0);  // DEL on empty still sends 1,0
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testKeyCodesWhileTypingText() throws Throwable {
        DOMUtils.focusNode(mContentViewCore, "textarea");
        assertWaitForKeyboardStatus(true);

        // The calls below are a reflection of what the Hacker's Keyboard sends  when the noted
        // key is touched on screen.  Exercise care when altering to make sure that the test
        // reflects reality.
        mConnection = (TestAdapterInputConnection) getAdapterInputConnection();
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 0, "", 0, 0, -1, -1);

        // H
        expectUpdateStateCall(mConnection);
        commitText(mConnection, "h", 1);
        assertEquals(KeyEvent.KEYCODE_H, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // O
        expectUpdateStateCall(mConnection);
        commitText(mConnection, "o", 1);
        assertEquals(KeyEvent.KEYCODE_O, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        expectUpdateStateCall(mConnection);
        deleteSurroundingText(mConnection, 1, 0);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // I
        expectUpdateStateCall(mConnection);
        commitText(mConnection, "i", 1);
        assertEquals(KeyEvent.KEYCODE_I, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("hi", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("hi", mConnection.getTextBeforeCursor(9, 0));

        // SPACE
        expectUpdateStateCall(mConnection);
        commitText(mConnection, " ", 1);
        assertEquals(KeyEvent.KEYCODE_SPACE, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("hi ", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("hi ", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        expectUpdateStateCall(mConnection);
        deleteSurroundingText(mConnection, 1, 0);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("hi", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("hi", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        expectUpdateStateCall(mConnection);
        deleteSurroundingText(mConnection, 1, 0);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        expectUpdateStateCall(mConnection);
        deleteSurroundingText(mConnection, 1, 0);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));

        // DEL (on empty input)
        deleteSurroundingText(mConnection, 1, 0);  // DEL on empty still sends 1,0
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testEnterKeyEventWhileComposingText() throws Throwable {
        DOMUtils.focusNode(mContentViewCore, "input_radio");
        assertWaitForKeyboardStatus(false);
        DOMUtils.focusNode(mContentViewCore, "textarea");
        assertWaitForKeyboardStatus(true);

        mConnection = (TestAdapterInputConnection) getAdapterInputConnection();
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 0, "", 0, 0, -1, -1);

        setComposingText(mConnection, "hello", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, 0, 5);

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mConnection.sendKeyEvent(
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
                mConnection.sendKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
            }
        });

        // TODO(aurimas): remove this workaround when crbug.com/278584 is fixed.
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "hello", 5, 5, -1, -1);
        // The second new line is not a user visible/editable one, it is a side-effect of Blink
        // using <br> internally.
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 3, "hello\n\n", 6, 6, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testTransitionsWhileComposingText() throws Throwable {
        DOMUtils.focusNode(mContentViewCore, "textarea");
        assertWaitForKeyboardStatus(true);

        mConnection = (TestAdapterInputConnection) getAdapterInputConnection();
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 0, "", 0, 0, -1, -1);

        // H
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "h", 1);
        assertEquals(KeyEvent.KEYCODE_H, mImeAdapter.mLastSyntheticKeyCode);

        // Simulate switch of input fields.
        finishComposingText(mConnection);

        // H
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "h", 1);
        assertEquals(KeyEvent.KEYCODE_H, mImeAdapter.mLastSyntheticKeyCode);
    }

    private void performGo(final AdapterInputConnection inputConnection,
            TestCallbackHelperContainer testCallbackHelperContainer) throws Throwable {
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
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return show == getImeAdapter().mIsShowWithoutHideOutstanding &&
                        (!show || getAdapterInputConnection() != null);
            }
        }));
    }

    private void waitAndVerifyEditableCallback(final ArrayList<TestImeState> states,
            final int index, String text, int selectionStart, int selectionEnd,
            int compositionStart, int compositionEnd) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return states.size() > index;
            }
        }));
        states.get(index).assertEqualState(
                text, selectionStart, selectionEnd, compositionStart, compositionEnd);
    }

    private void expectUpdateStateCall(final TestAdapterInputConnection connection) {
        connection.mImeUpdateQueue.clear();
    }

    private void assertUpdateStateCall(final TestAdapterInputConnection connection, int maxms)
            throws Exception {
        while (connection.mImeUpdateQueue.size() == 0 && maxms > 0) {
            try {
                Thread.sleep(50);
            } catch (Exception e) {
                // Not really a problem since we're just going to sleep again.
            }
            maxms -= 50;
        }
        assertTrue(connection.mImeUpdateQueue.size() > 0);
    }

    private void assertClipboardContents(final Activity activity, final String expectedContents)
            throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        ClipboardManager clipboardManager =
                                (ClipboardManager) activity.getSystemService(
                                        Context.CLIPBOARD_SERVICE);
                        ClipData clip = clipboardManager.getPrimaryClip();
                        return clip != null && clip.getItemCount() == 1
                                && TextUtils.equals(clip.getItemAt(0).getText(), expectedContents);
                    }
                });
            }
        }));
    }

    private ImeAdapter getImeAdapter() {
        return mContentViewCore.getImeAdapterForTest();
    }

    private AdapterInputConnection getAdapterInputConnection() {
        return mContentViewCore.getInputConnectionForTest();
    }

    private void copy(final ImeAdapter adapter) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                adapter.copy();
            }
        });
    }

    private void cut(final ImeAdapter adapter) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                adapter.cut();
            }
        });
    }

    private void paste(final ImeAdapter adapter) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                adapter.paste();
            }
        });
    }

    private void selectAll(final ImeAdapter adapter) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                adapter.selectAll();
            }
        });
    }

    private void unselect(final ImeAdapter adapter) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                adapter.unselect();
            }
        });
    }

    private void commitText(final AdapterInputConnection connection, final CharSequence text,
            final int newCursorPosition) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.commitText(text, newCursorPosition);
            }
        });
    }

    private void setSelection(final AdapterInputConnection connection, final int start,
            final int end) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.setSelection(start, end);
            }
        });
    }

    private void setComposingRegion(final AdapterInputConnection connection, final int start,
            final int end) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.setComposingRegion(start, end);
            }
        });
    }

    private void setComposingText(final AdapterInputConnection connection, final CharSequence text,
            final int newCursorPosition) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.setComposingText(text, newCursorPosition);
            }
        });
    }

    private void finishComposingText(final AdapterInputConnection connection) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.finishComposingText();
            }
        });
    }

    private void deleteSurroundingText(final AdapterInputConnection connection, final int before,
            final int after) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.deleteSurroundingText(before, after);
            }
        });
    }

    private static class TestAdapterInputConnectionFactory extends
            ImeAdapter.AdapterInputConnectionFactory {
        @Override
        public AdapterInputConnection get(View view, ImeAdapter imeAdapter,
                Editable editable, EditorInfo outAttrs) {
            return new TestAdapterInputConnection(view, imeAdapter, editable, outAttrs);
        }
    }

    private static class TestAdapterInputConnection extends AdapterInputConnection {
        private final ArrayList<TestImeState> mImeUpdateQueue = new ArrayList<TestImeState>();

        public TestAdapterInputConnection(View view, ImeAdapter imeAdapter,
                Editable editable, EditorInfo outAttrs) {
            super(view, imeAdapter, editable, outAttrs);
        }

        @Override
        public void updateState(String text, int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd, boolean requiredAck) {
            mImeUpdateQueue.add(new TestImeState(text, selectionStart, selectionEnd,
                    compositionStart, compositionEnd));
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
