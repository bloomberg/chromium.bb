// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.spy;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Log;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * A robolectric test for {@link AutocompleteEditText} class.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutocompleteEditTextTest {
    private static final String TAG = "cr_AutocompleteEdit";
    private static final boolean DEBUG = false;

    private InOrder mInOrder;
    private AutocompleteEditText mAutocomplete;
    private Context mContext;
    private InputConnection mInputConnection;
    private BaseInputConnection mDummyTargetInputConnection;
    private AutocompleteEmbedder mEmbedder;

    // This is needed to limit the target of inOrder#verify.
    private static class AutocompleteEmbedder {
        public void onAutocompleteTextStateChanged(boolean textDeleted, boolean updateDisplay) {
            if (DEBUG) Log.i(TAG, "onAutocompleteTextStateChanged");
        }
    }

    private static class TestAutocompleteEditText extends AutocompleteEditText {
        private AutocompleteEmbedder mEmbedder;

        public TestAutocompleteEditText(
                AutocompleteEmbedder embedder, Context context, AttributeSet attrs) {
            super(context, attrs);
            mEmbedder = embedder;
        }

        @Override
        public void onAutocompleteTextStateChanged(boolean textDeleted, boolean updateDisplay) {
            // This function is called in super(), so mEmbedder may be null.
            if (mEmbedder != null) {
                mEmbedder.onAutocompleteTextStateChanged(textDeleted, updateDisplay);
            }
        }

        @Override
        public boolean hasFocus() {
            return true;
        }
    }

    @Before
    public void setUp() throws Exception {
        ShadowLog.stream = System.out;
        if (DEBUG) Log.i(TAG, "setUp started.");
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mEmbedder = spy(new AutocompleteEmbedder());
        mAutocomplete = spy(new TestAutocompleteEditText(mEmbedder, mContext, null));
        assertNotNull(mAutocomplete);
        // Note: this cannot catch the first {@link
        // AutocompleteEditText#onAutocompleteTextStateChanged(boolean, boolean)}, which is caused
        // by View constructor's call to setText().
        mInOrder = inOrder(mEmbedder);
        mAutocomplete.onCreateInputConnection(new EditorInfo());
        mInputConnection = mAutocomplete.getInputConnection();
        assertNotNull(mInputConnection);
        mInOrder.verifyNoMoreInteractions();
        if (DEBUG) Log.i(TAG, "setUp finished.");
    }

    private void assertTexts(String userText, String autocompleteText) {
        assertEquals(userText, mAutocomplete.getTextWithoutAutocomplete());
        assertEquals(userText + autocompleteText, mAutocomplete.getTextWithAutocomplete());
        assertEquals(autocompleteText.length(), mAutocomplete.getAutocompleteLength());
        assertEquals(!TextUtils.isEmpty(autocompleteText), mAutocomplete.hasAutocomplete());
    }

    @Test
    public void testAppend_CommitText() {
        // Feeder should call this at the beginning.
        mAutocomplete.setIgnoreTextChangesForAutocomplete(false);
        // User types "h".
        assertTrue(mInputConnection.commitText("h", 1));
        mInOrder.verify(mEmbedder).onAutocompleteTextStateChanged(false, true);
        // User types "hello".
        assertTrue(mInputConnection.commitText("ello", 1));
        mInOrder.verify(mEmbedder).onAutocompleteTextStateChanged(false, true);
        mInOrder.verifyNoMoreInteractions();
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world");
        assertTexts("hello", " world");
        assertTrue(mInputConnection.beginBatchEdit());
        assertTrue(mInputConnection.commitText(" ", 1));
        assertTexts("hello ", "world");
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mInputConnection.endBatchEdit());
        mInOrder.verify(mEmbedder).onAutocompleteTextStateChanged(false, true);
        mAutocomplete.setAutocompleteText("hello ", "world");
        assertTexts("hello ", "world");
        mInOrder.verifyNoMoreInteractions();
    }
}