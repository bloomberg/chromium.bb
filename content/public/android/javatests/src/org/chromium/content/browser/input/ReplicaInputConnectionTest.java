// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.ResultReceiver;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.Editable;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.input.ReplicaInputConnection.ImeState;
import org.chromium.content_shell_apk.ContentShellTestBase;
import org.chromium.ui.base.ime.TextInputType;

import java.util.ArrayList;

/**
 * Tests AdapterInputConnection class and its callbacks to ImeAdapter.
 */
public class ReplicaInputConnectionTest extends ContentShellTestBase {
    private ReplicaInputConnection mConnection;
    private TestInputMethodManagerWrapper mWrapper;
    private TestImeAdapter mImeAdapter;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        launchContentShellWithUrl("about:blank");
        waitForActiveShellToBeDoneLoading();
        mWrapper = new TestInputMethodManagerWrapper(getActivity());
        TestImeAdapterDelegate imeAdapterDelegate =
                new TestImeAdapterDelegate(getContentViewCore().getContainerView());
        mImeAdapter = new TestImeAdapter(mWrapper, imeAdapterDelegate);
        Editable editable = Editable.Factory.getInstance().newEditable("");
        Handler handler = new Handler(Looper.getMainLooper());

        mConnection = new ReplicaInputConnection(getContentViewCore().getContainerView(),
                mImeAdapter, handler, editable, TextInputType.TEXT, 0, 0, new EditorInfo());
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testAdjustLengthBeforeAndAfterSelection() throws Throwable {
        final String ga = "\uAC00";
        final String smiley = "\uD83D\uDE0A"; // multi character codepoint

        // No need to adjust length.
        assertFalse(ReplicaInputConnection.isIndexBetweenUtf16SurrogatePair("a", 0));
        assertFalse(ReplicaInputConnection.isIndexBetweenUtf16SurrogatePair(ga, 0));
        assertFalse(ReplicaInputConnection.isIndexBetweenUtf16SurrogatePair("aa", 1));
        assertFalse(ReplicaInputConnection.isIndexBetweenUtf16SurrogatePair("a" + smiley + "a", 1));

        // Needs to adjust length.
        assertTrue(ReplicaInputConnection.isIndexBetweenUtf16SurrogatePair(smiley, 1));
        assertTrue(ReplicaInputConnection.isIndexBetweenUtf16SurrogatePair(smiley + "a", 1));
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testSetComposingText() throws Throwable {
        assertEquals(0, mWrapper.getUpdateSelectionCallCount());

        mConnection.setComposingText("t", 1);
        assertCorrectState("t", 1, 1, 0, 1, mConnection.getImeStateForTesting());
        mWrapper.verifyUpdateSelectionCall(0, 1, 1, 0, 1);
        // BaseInputConnection calls updateSelection() one more time because getEditable() does
        // not return a correct value here.
        mWrapper.verifyUpdateSelectionCall(1, 1, 1, 0, 1);
        assertEquals(2, mWrapper.getUpdateSelectionCallCount());

        mConnection.setComposingText("te", 1);
        assertCorrectState("te", 2, 2, 0, 2, mConnection.getImeStateForTesting());
        mWrapper.verifyUpdateSelectionCall(2, 2, 2, 0, 2);
        mWrapper.verifyUpdateSelectionCall(3, 2, 2, 0, 2);

        mConnection.setComposingText("tes", 1);
        assertCorrectState("tes", 3, 3, 0, 3, mConnection.getImeStateForTesting());
        mWrapper.verifyUpdateSelectionCall(4, 3, 3, 0, 3);
        mWrapper.verifyUpdateSelectionCall(5, 3, 3, 0, 3);

        mConnection.setComposingText("test", 1);
        assertCorrectState("test", 4, 4, 0, 4, mConnection.getImeStateForTesting());
        mWrapper.verifyUpdateSelectionCall(6, 4, 4, 0, 4);
        mWrapper.verifyUpdateSelectionCall(7, 4, 4, 0, 4);
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
        mWrapper.verifyUpdateSelectionCall(0, 4, 4, 0, 4);
    }

    private static class TestImeAdapter extends ImeAdapter {
        private final ArrayList<Integer> mKeyEventQueue = new ArrayList<Integer>();

        public TestImeAdapter(InputMethodManagerWrapper wrapper, ImeAdapterDelegate embedder) {
            super(wrapper, embedder);
        }

        @Override
        public void sendSyntheticKeyPress(int keyCode, int flags) {
            mKeyEventQueue.add(keyCode);
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
        public boolean hideSoftInputFromWindow(
                IBinder windowToken, int flags, ResultReceiver resultReceiver) {
            return true;
        }

        @Override
        public void updateSelection(
                View view, int selStart, int selEnd, int candidatesStart, int candidatesEnd) {
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
            assertEquals(
                    "Composition start did not match", compositionStart, state.compositionStart);
            assertEquals("Composition end did not match", compositionEnd, state.compositionEnd);
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