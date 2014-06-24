// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;
import android.view.inputmethod.EditorInfo;

import org.chromium.content.browser.input.AdapterInputConnection;
import org.chromium.content.browser.input.ImeAdapter;
import org.chromium.content.browser.input.InputMethodManagerWrapper;
import org.chromium.content.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Tests that when InputConnection is recreated, the text is still retained.
 */
public class ContentViewCoreInputConnectionTest extends ContentShellTestBase {
    private ContentViewCore mContentViewCore;
    private TestImeAdapter mImeAdapter;
    private TestInputMethodManagerWrapper mInputMethodManagerWrapper;

    private static class TestImeAdapter extends ImeAdapter {
        public TestImeAdapter(InputMethodManagerWrapper immw) {
            super(immw, null);
        }
        @Override
        public boolean hasTextInputType() {
            return true;
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentViewCore = new ContentViewCore(getActivity());
        mInputMethodManagerWrapper = new TestInputMethodManagerWrapper(mContentViewCore);
        mImeAdapter = new TestImeAdapter(mInputMethodManagerWrapper);
        mImeAdapter.setInputMethodManagerWrapper(new TestInputMethodManagerWrapper(
            mContentViewCore));
        mContentViewCore.setImeAdapterForTest(mImeAdapter);
        mContentViewCore.setContainerView(getActivity().getActiveShell().getContentView());
    }

    /**
     * When creating a new InputConnection (e.g. after switching software keyboard), make sure the
     * text content in the Editable is not lost.
     */
    @SmallTest
    @RerunWithUpdatedContainerView
    public void testRecreateInputConnection() throws Exception {
        EditorInfo info = new EditorInfo();

        mContentViewCore.onCreateInputConnection(info);
        AdapterInputConnection adapter = mContentViewCore.getAdapterInputConnectionForTest();
        adapter.updateState("Is this text restored?", 0, 0, 0, 0, true);

        String text = mContentViewCore.getEditableForTest().toString();
        assertEquals("Check if the initial text is stored.", "Is this text restored?", text);

        // Create a new InputConnection.
        EditorInfo info2 = new EditorInfo();
        mContentViewCore.onCreateInputConnection(info2);

        String newtext = mContentViewCore.getEditableForTest().toString();
        assertEquals("Check if the string is restored.", "Is this text restored?", newtext);
    }
}
