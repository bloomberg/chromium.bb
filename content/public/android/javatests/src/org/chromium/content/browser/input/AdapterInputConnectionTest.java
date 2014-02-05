// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.os.IBinder;
import android.os.ResultReceiver;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.input.AdapterInputConnection.ImeState;
import org.chromium.content.browser.input.ImeAdapter.ImeAdapterDelegate;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.ArrayList;

/**
 * Tests AdapterInputConnection class and its callbacks to ImeAdapter.
 */
public class AdapterInputConnectionTest extends ContentShellTestBase {

    private AdapterInputConnection mConnection;
    private TestInputMethodManagerWrapper mWrapper;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        launchContentShellWithUrl("about:blank");
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
        mWrapper = new TestInputMethodManagerWrapper(getActivity());
        ImeAdapterDelegate delegate = new TestImeAdapterDelegate();
        ImeAdapter imeAdapter = new TestImeAdapter(mWrapper, delegate);
        EditorInfo info = new EditorInfo();
        mConnection = new AdapterInputConnection(
                getActivity().getActiveContentView(), imeAdapter, info);
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testSetComposingText() throws Throwable {
        mConnection.setComposingText("t", 1);
        assertCorrectState("t", 1, 1, 0, 1, mConnection.getImeStateForTesting());
        mWrapper.verifyUpdateSelectionCall(0, 1, 1, 0 ,1);

        mConnection.setComposingText("te", 1);
        assertCorrectState("te", 2, 2, 0, 2, mConnection.getImeStateForTesting());
        mWrapper.verifyUpdateSelectionCall(1, 2, 2, 0 ,2);

        mConnection.setComposingText("tes", 1);
        assertCorrectState("tes", 3, 3, 0, 3, mConnection.getImeStateForTesting());
        mWrapper.verifyUpdateSelectionCall(2, 3, 3, 0, 3);

        mConnection.setComposingText("test", 1);
        assertCorrectState("test", 4, 4, 0, 4, mConnection.getImeStateForTesting());
        mWrapper.verifyUpdateSelectionCall(3, 4, 4, 0, 4);
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testSelectionUpdatesDuringBatch() throws Throwable {
        mConnection.beginBatchEdit();
        mConnection.setComposingText("t", 1);
        assertEquals(0, mWrapper.getUpdateSelectionCallCount());
        mConnection.setComposingText("te", 1);
        assertEquals(0, mWrapper.getUpdateSelectionCallCount());
        mConnection.beginBatchEdit();
        mConnection.setComposingText("tes", 1);
        assertEquals(0, mWrapper.getUpdateSelectionCallCount());
        mConnection.endBatchEdit();
        mConnection.setComposingText("test", 1);
        assertEquals(0, mWrapper.getUpdateSelectionCallCount());
        mConnection.endBatchEdit();
        assertEquals(1, mWrapper.getUpdateSelectionCallCount());
        mWrapper.verifyUpdateSelectionCall(0, 4, 4, 0 ,4);
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testDismissInputMethodWindowAfterFinishingTyping() throws Throwable {
        assertEquals(false, mWrapper.isHidden());

        mConnection.performEditorAction(EditorInfo.IME_ACTION_NEXT);
        assertEquals(false, mWrapper.isHidden());
        mWrapper.showSoftInput(null, 0, null);

        mConnection.performEditorAction(EditorInfo.IME_ACTION_SEND);
        assertEquals(false, mWrapper.isHidden());
        mWrapper.showSoftInput(null, 0, null);

        mConnection.performEditorAction(EditorInfo.IME_ACTION_GO);
        assertEquals(true, mWrapper.isHidden());
        mWrapper.showSoftInput(null, 0, null);

        mConnection.performEditorAction(EditorInfo.IME_ACTION_DONE);
        assertEquals(true, mWrapper.isHidden());
        mWrapper.showSoftInput(null, 0, null);

        mConnection.performEditorAction(EditorInfo.IME_ACTION_SEARCH);
        assertEquals(true, mWrapper.isHidden());
        mWrapper.showSoftInput(null, 0, null);
    }

    private static class TestImeAdapter extends ImeAdapter {
        public TestImeAdapter(InputMethodManagerWrapper wrapper, ImeAdapterDelegate embedder) {
            super(wrapper, embedder);
        }
    }

    private static class TestInputMethodManagerWrapper extends InputMethodManagerWrapper {
        private final ArrayList<ImeState> mUpdates = new ArrayList<ImeState>();
        private boolean hidden = false;

        public TestInputMethodManagerWrapper(Context context) {
            super(context);
        }

        @Override
        public void restartInput(View view) {}

        @Override
        public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {
            hidden = false;
        }

        @Override
        public boolean isActive(View view) {
            return true;
        }

        @Override
        public boolean hideSoftInputFromWindow(IBinder windowToken, int flags,
                ResultReceiver resultReceiver) {
            hidden = true;
            return true;
        }

        @Override
        public void updateSelection(View view, int selStart, int selEnd,
                int candidatesStart, int candidatesEnd) {
          mUpdates.add(new ImeState("", selStart, selEnd, candidatesStart, candidatesEnd));
        }

        public int getUpdateSelectionCallCount() {
            return mUpdates.size();
        }

        public void verifyUpdateSelectionCall(int index, int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd) {
            ImeState state = mUpdates.get(index);
            assertEquals("Selection start did not match", selectionStart, state.selectionStart);
            assertEquals("Selection end did not match", selectionEnd, state.selectionEnd);
            assertEquals("Composition start did not match", compositionStart,
                    state.compositionStart);
            assertEquals("Composition end did not match", compositionEnd, state.compositionEnd);
        }

        public boolean isHidden() {
            return hidden;
        }
    }

    private static class TestImeAdapterDelegate implements ImeAdapterDelegate {
        @Override
        public void onImeEvent(boolean isFinish) {}

        @Override
        public void onSetFieldValue() {}

        @Override
        public void onDismissInput() {}

        @Override
        public View getAttachedView() {
            return null;
        }

        @Override
        public ResultReceiver getNewShowKeyboardReceiver() {
            return null;
        }
    }

    private static void assertCorrectState(String text, int selectionStart, int selectionEnd,
            int compositionStart, int compositionEnd, ImeState actual) {
        assertEquals("Text did not match", text, actual.text);
        assertEquals("Selection start did not match", selectionStart, actual.selectionStart);
        assertEquals("Selection end did not match", selectionEnd, actual.selectionEnd);
        assertEquals("Composition start did not match", compositionStart, actual.compositionStart);
        assertEquals("Composition end did not match", compositionEnd, actual.compositionEnd);
    }
}
