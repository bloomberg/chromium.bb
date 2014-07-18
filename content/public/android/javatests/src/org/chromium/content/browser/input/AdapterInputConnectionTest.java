// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.os.IBinder;
import android.os.ResultReceiver;
import android.test.suitebuilder.annotation.MediumTest;
import android.text.Editable;
import android.text.Selection;
import android.view.KeyEvent;
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
    private Editable mEditable;
    private TestImeAdapter mImeAdapter;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        launchContentShellWithUrl("about:blank");
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
        mWrapper = new TestInputMethodManagerWrapper(getActivity());
        ImeAdapterDelegate delegate = new TestImeAdapterDelegate();
        mImeAdapter = new TestImeAdapter(mWrapper, delegate);
        EditorInfo info = new EditorInfo();
        mEditable = Editable.Factory.getInstance().newEditable("");
        mConnection = new AdapterInputConnection(
                getContentViewCore().getContainerView(), mImeAdapter, mEditable, info);
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    @RerunWithUpdatedContainerView
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
    public void testDeleteSurroundingText() throws Throwable {
        // Tests back deletion of a single character with empty input.
        mConnection.deleteSurroundingText(1, 0);
        assertEquals(0, mImeAdapter.getDeleteSurroundingTextCallCount());
        Integer[] keyEvents = mImeAdapter.getKeyEvents();
        assertEquals(1, keyEvents.length);
        assertEquals(KeyEvent.KEYCODE_DEL, keyEvents[0].intValue());

        // Tests forward deletion of a single character with non-empty input.
        mEditable.replace(0, mEditable.length(), " hello");
        Selection.setSelection(mEditable, 0, 0);
        mConnection.deleteSurroundingText(0, 1);
        assertEquals(0, mImeAdapter.getDeleteSurroundingTextCallCount());
        keyEvents = mImeAdapter.getKeyEvents();
        assertEquals(2, keyEvents.length);
        assertEquals(KeyEvent.KEYCODE_FORWARD_DEL, keyEvents[1].intValue());

        // Tests back deletion of multiple characters with non-empty input.
        mEditable.replace(0, mEditable.length(), "hello ");
        Selection.setSelection(mEditable, mEditable.length(), mEditable.length());
        mConnection.deleteSurroundingText(2, 0);
        assertEquals(1, mImeAdapter.getDeleteSurroundingTextCallCount());
        assertEquals(2, mImeAdapter.getKeyEvents().length);
    }

    private static class TestImeAdapter extends ImeAdapter {
        private final ArrayList<Integer> mKeyEventQueue = new ArrayList<Integer>();
        private int mDeleteSurroundingTextCounter;

        public TestImeAdapter(InputMethodManagerWrapper wrapper, ImeAdapterDelegate embedder) {
            super(wrapper, embedder);
        }

        @Override
        public boolean deleteSurroundingText(int beforeLength, int afterLength) {
            ++mDeleteSurroundingTextCounter;
            return true;
        }

        @Override
        public void sendKeyEventWithKeyCode(int keyCode, int flags) {
            mKeyEventQueue.add(keyCode);
        }

        public int getDeleteSurroundingTextCallCount() {
            return mDeleteSurroundingTextCounter;
        }

        public Integer[] getKeyEvents() {
            return mKeyEventQueue.toArray(new Integer[mKeyEventQueue.size()]);
        }
    }

    private static class TestInputMethodManagerWrapper extends InputMethodManagerWrapper {
        private final ArrayList<ImeState> mUpdates = new ArrayList<ImeState>();

        public TestInputMethodManagerWrapper(Context context) {
            super(context);
        }

        @Override
        public void restartInput(View view) {}

        @Override
        public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {}

        @Override
        public boolean isActive(View view) {
            return true;
        }

        @Override
        public boolean hideSoftInputFromWindow(IBinder windowToken, int flags,
                ResultReceiver resultReceiver) {
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
    }

    private static class TestImeAdapterDelegate implements ImeAdapterDelegate {
        @Override
        public void onImeEvent(boolean isFinish) {}

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
