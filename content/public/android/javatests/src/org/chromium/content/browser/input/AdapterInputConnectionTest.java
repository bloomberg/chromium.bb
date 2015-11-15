// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.os.IBinder;
import android.os.ResultReceiver;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.input.AdapterInputConnection.ImeState;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.ArrayList;

/**
 * Tests AdapterInputConnection class and its callbacks to ImeAdapter.
 */
public class AdapterInputConnectionTest extends ContentShellTestBase {

    private AdapterInputConnection mConnection;
    private TestInputMethodManagerWrapper mWrapper;
    private TestImeAdapter mImeAdapter;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        launchContentShellWithUrl("about:blank");
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
        mWrapper = new TestInputMethodManagerWrapper(getActivity());
        mImeAdapter = new TestImeAdapter(mWrapper, new TestImeAdapterDelegate());
        mConnection = new AdapterInputConnection(
                getContentViewCore().getContainerView(), mImeAdapter, 0, 0, new EditorInfo());
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testAdjustLengthBeforeAndAfterSelection() throws Throwable {
        final String ga = "\uAC00";
        final String smiley = "\uD83D\uDE0A"; // multi character codepoint

        // No need to adjust length.
        assertFalse(AdapterInputConnection.isIndexBetweenUtf16SurrogatePair("a", 0));
        assertFalse(AdapterInputConnection.isIndexBetweenUtf16SurrogatePair(ga, 0));
        assertFalse(AdapterInputConnection.isIndexBetweenUtf16SurrogatePair("aa", 1));
        assertFalse(AdapterInputConnection.isIndexBetweenUtf16SurrogatePair("a" + smiley + "a", 1));

        // Needs to adjust length.
        assertTrue(AdapterInputConnection.isIndexBetweenUtf16SurrogatePair(smiley, 1));
        assertTrue(AdapterInputConnection.isIndexBetweenUtf16SurrogatePair(smiley + "a", 1));
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    @RerunWithUpdatedContainerView
    public void testSetComposingText() throws Throwable {
        mConnection.setComposingText("t", 1);
        assertCorrectState("t", 1, 1, 0, 1, mConnection.getImeStateForTesting());
        mWrapper.verifyUpdateSelectionCall(0, 1, 1, 0, 1);

        mConnection.setComposingText("te", 1);
        assertCorrectState("te", 2, 2, 0, 2, mConnection.getImeStateForTesting());
        mWrapper.verifyUpdateSelectionCall(1, 2, 2, 0, 2);

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

    private static void assertCorrectState(String text, int selectionStart, int selectionEnd,
            int compositionStart, int compositionEnd, ImeState actual) {
        assertEquals("Text did not match", text, actual.text);
        assertEquals("Selection start did not match", selectionStart, actual.selectionStart);
        assertEquals("Selection end did not match", selectionEnd, actual.selectionEnd);
        assertEquals("Composition start did not match", compositionStart, actual.compositionStart);
        assertEquals("Composition end did not match", compositionEnd, actual.compositionEnd);
    }
}