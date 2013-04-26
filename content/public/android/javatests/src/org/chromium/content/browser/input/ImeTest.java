// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.IBinder;
import android.os.ResultReceiver;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.ArrayList;
import java.util.concurrent.Callable;

public class ImeTest extends ContentShellTestBase {

    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\"" +
            "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>" +
            "<body><form action=\"about:blank\">" +
            "<input id=\"input_text\" type=\"text\" />" +
            "<input id=\"input_radio\" type=\"radio\" />" +
            "</form></body></html>");

    private TestAdapterInputConnection mConnection;
    private ImeAdapter mImeAdapter;
    private ContentView mContentView;
    private TestCallbackHelperContainer mCallbackContainer;
    private TestInputMethodManagerWrapper mInputMethodManagerWrapper;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        launchContentShellWithUrl(DATA_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());

        mInputMethodManagerWrapper = new TestInputMethodManagerWrapper(getContentViewCore());
        getImeAdapter().setInputMethodManagerWrapper(mInputMethodManagerWrapper);
        assertEquals(0, mInputMethodManagerWrapper.mShowSoftInputCounter);
        getContentViewCore().setAdapterInputConnectionFactory(
                new TestAdapterInputConnectionFactory());

        mContentView = getActivity().getActiveContentView();
        mCallbackContainer = new TestCallbackHelperContainer(mContentView);
        // TODO(aurimas) remove this wait once crbug.com/179511 is fixed.
        assertWaitForPageScaleFactor(1);
        DOMUtils.clickNode(this, mContentView, mCallbackContainer, "input_text");
        assertWaitForKeyboardStatus(true);

        mConnection = (TestAdapterInputConnection) getAdapterInputConnection();
        mImeAdapter = getImeAdapter();

        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 0, "", 0, 0, -1, -1);
        assertEquals(1, mInputMethodManagerWrapper.mShowSoftInputCounter);
        assertEquals(0, mInputMethodManagerWrapper.mEditorInfo.initialSelStart);
        assertEquals(0, mInputMethodManagerWrapper.mEditorInfo.initialSelEnd);
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testKeyboardDismissedAfterClickingGo() throws Throwable {
        mConnection.setComposingText("hello", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, 0, 5);

        performGo(getAdapterInputConnection(), mCallbackContainer);

        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "", 0, 0, -1, -1);
        assertWaitForKeyboardStatus(false);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testGetTextUpdatesAfterEnteringText() throws Throwable {
        mConnection.setComposingText("h", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "h", 1, 1, 0, 1);
        assertEquals(1, mInputMethodManagerWrapper.mShowSoftInputCounter);

        mConnection.setComposingText("he", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "he", 2, 2, 0, 2);
        assertEquals(1, mInputMethodManagerWrapper.mShowSoftInputCounter);

        mConnection.setComposingText("hel", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 3, "hel", 3, 3, 0, 3);
        assertEquals(1, mInputMethodManagerWrapper.mShowSoftInputCounter);

        mConnection.commitText("hel", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 4, "hel", 3, 3, -1, -1);
        assertEquals(1, mInputMethodManagerWrapper.mShowSoftInputCounter);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeCopy() throws Exception {
        mConnection.commitText("hello", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, -1, -1);

        mConnection.setSelection(2, 5);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "hello", 2, 5, -1, -1);

        mImeAdapter.copy();
        assertClipboardContents(getActivity(), "llo");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testEnterTextAndRefocus() throws Exception {
        mConnection.commitText("hello", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, -1, -1);

        DOMUtils.clickNode(this, mContentView, mCallbackContainer, "input_radio");
        assertWaitForKeyboardStatus(false);

        DOMUtils.clickNode(this, mContentView, mCallbackContainer, "input_text");
        assertWaitForKeyboardStatus(true);
        assertEquals(5, mInputMethodManagerWrapper.mEditorInfo.initialSelStart);
        assertEquals(5, mInputMethodManagerWrapper.mEditorInfo.initialSelEnd);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeCut() throws Exception {
        mConnection.commitText("snarful", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "snarful", 7, 7, -1, -1);

        mConnection.setSelection(1, 5);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "snarful", 1, 5, -1, -1);

        mImeAdapter.cut();
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

        mImeAdapter.paste();
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "blarg", 5, 5, -1, -1);

        mConnection.setSelection(3, 5);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "blarg", 3, 5, -1, -1);

        mImeAdapter.paste();
        // Paste is a two step process when there is a non-zero selection.
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 3, "bla", 3, 3, -1, -1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 4, "blablarg", 8, 8, -1, -1);

        mImeAdapter.paste();
        waitAndVerifyEditableCallback(
                mConnection.mImeUpdateQueue, 5, "blablargblarg", 13, 13, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeSelectAndUnSelectAll() throws Exception {
        mConnection.commitText("hello", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, -1, -1);

        mImeAdapter.selectAll();
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "hello", 0, 5, -1, -1);

        mImeAdapter.unselect();
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 3, "", 0, 0, -1, -1);

        assertWaitForKeyboardStatus(false);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testUpdatesGetIgnoredDuringBatchEdits() throws Throwable {
        mConnection.beginBatchEdit();
        assertWaitForSetIgnoreUpdates(true, mConnection);

        mConnection.setComposingText("h", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "h", 1, 1, 0, 1);
        assertTrue(mConnection.isIgnoringTextInputStateUpdates());

        mConnection.setComposingText("he", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "he", 2, 2, 0, 2);
        assertTrue(mConnection.isIgnoringTextInputStateUpdates());

        mConnection.setComposingText("hel", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 3, "hel", 3, 3, 0, 3);

        assertEquals(0, mConnection.mUpdateSelectionCounter);
        assertTrue(mConnection.isIgnoringTextInputStateUpdates());
        mConnection.endBatchEdit();
        assertWaitForSetIgnoreUpdates(false, mConnection);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testShowImeIfNeeded() throws Throwable {
        DOMUtils.focusNode(this, mContentView, mCallbackContainer, "input_radio");
        assertWaitForKeyboardStatus(false);

        performShowImeIfNeeded();
        assertWaitForKeyboardStatus(false);

        DOMUtils.focusNode(this, mContentView, mCallbackContainer, "input_text");
        assertWaitForKeyboardStatus(false);

        performShowImeIfNeeded();
        assertWaitForKeyboardStatus(true);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testFinishComposingText() throws Throwable {
        mConnection.commitText("hllo", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 1, "hllo", 4, 4, -1, -1);

        mConnection.commitText(" ", 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 2, "hllo ", 5, 5, -1, -1);

        mConnection.setSelection(1, 1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 3, "hllo ", 1, 1, -1, -1);

        mConnection.setComposingRegion(0, 4);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 4, "hllo ", 1, 1, 0, 4);

        mConnection.finishComposingText();
        // finishComposingText() is a two step IME event.
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 5, "hllo ", 4, 4, -1, -1);
        waitAndVerifyEditableCallback(mConnection.mImeUpdateQueue, 6, "hllo ", 1, 1, -1, -1);
    }

    private void performShowImeIfNeeded() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mContentView.getContentViewCore().showImeIfNeeded();
            }
        });
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



    private void assertWaitForPageScaleFactor(final float scale) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getContentViewCore().getScale() == scale;
            }
        }));
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

    private void assertWaitForSetIgnoreUpdates(final boolean ignore,
            final TestAdapterInputConnection connection) throws Throwable {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ignore == connection.isIgnoringTextInputStateUpdates();
            }
        }));
    }

    private ImeAdapter getImeAdapter() {
        return getContentViewCore().getImeAdapterForTest();
    }

    private AdapterInputConnection getAdapterInputConnection() {
        return getContentViewCore().getInputConnectionForTest();
    }

    private static class TestAdapterInputConnectionFactory extends
            ImeAdapter.AdapterInputConnectionFactory {
        @Override
        public AdapterInputConnection get(View view, ImeAdapter imeAdapter,
                EditorInfo outAttrs) {
            return new TestAdapterInputConnection(view, imeAdapter, outAttrs);
        }
    }

    private static class TestAdapterInputConnection extends AdapterInputConnection {
        private int mUpdateSelectionCounter = 0;
        private ArrayList<TestImeState> mImeUpdateQueue = new ArrayList<ImeTest.TestImeState>();

        public TestAdapterInputConnection(View view, ImeAdapter imeAdapter, EditorInfo outAttrs) {
            super(view, imeAdapter, outAttrs);
        }

        @Override
        public void setEditableText(String text, int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd) {
            mImeUpdateQueue.add(new TestImeState(text, selectionStart, selectionEnd,
                    compositionStart, compositionEnd));
            super.setEditableText(
                    text, selectionStart, selectionEnd, compositionStart, compositionEnd);
        }

        @Override
        protected void updateSelection(
                int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd) {
            mUpdateSelectionCounter++;
        }
    }

    private static class TestInputMethodManagerWrapper extends InputMethodManagerWrapper {
        private ContentViewCore mContentViewCore;
        private InputConnection mInputConnection;
        private int mShowSoftInputCounter = 0;
        private EditorInfo mEditorInfo;

        public TestInputMethodManagerWrapper(ContentViewCore contentViewCore) {
            super(null);
            mContentViewCore = contentViewCore;
        }

        @Override
        public void restartInput(View view) {
            mEditorInfo = new EditorInfo();
            mInputConnection = mContentViewCore.onCreateInputConnection(mEditorInfo);
        }

        @Override
        public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {
            mShowSoftInputCounter++;
            if (mInputConnection != null) return;
            mEditorInfo = new EditorInfo();
            mInputConnection = mContentViewCore.onCreateInputConnection(mEditorInfo);
        }

        @Override
        public boolean isActive(View view) {
            if (mInputConnection == null) return false;
            return true;
        }

        @Override
        public boolean hideSoftInputFromWindow(IBinder windowToken, int flags,
                ResultReceiver resultReceiver) {
            boolean retVal = mInputConnection == null;
            mInputConnection = null;
            return retVal;
        }

        @Override
        public void updateSelection(View view, int selStart, int selEnd,
                int candidatesStart, int candidatesEnd) {
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
            assertEquals("Text did not match", mText, text);
            assertEquals("Selection start did not match", mSelectionStart, selectionStart);
            assertEquals("Selection end did not match", mSelectionEnd, selectionEnd);
            assertEquals("Composition start did not match", mCompositionStart, compositionStart);
            assertEquals("Composition end did not match", mCompositionEnd, compositionEnd);
        }
    }
}
