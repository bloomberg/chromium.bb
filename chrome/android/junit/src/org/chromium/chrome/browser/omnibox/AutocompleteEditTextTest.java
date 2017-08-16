// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.spy;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * A robolectric test for {@link AutocompleteEditText} class.
 * TODO(changwan): switch to ParameterizedRobolectricTest once crbug.com/733324 is fixed.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutocompleteEditTextTest {
    private static final String TAG = "cr_AutocompleteTest";

    private static final boolean DEBUG = false;

    @Rule
    public Features.Processor mProcessor = new Features.Processor();

    private InOrder mInOrder;
    private AutocompleteEditText mAutocomplete;
    private Context mContext;
    private InputConnection mInputConnection;
    private Verifier mVerifier;

    // Limits the target of InOrder#verify.
    private static class Verifier {
        public void onAutocompleteTextStateChanged(boolean updateDisplay) {
            if (DEBUG) Log.i(TAG, "onAutocompleteTextStateChanged(%b)", updateDisplay);
        }

        public void onUpdateSelection(int selStart, int selEnd) {
            if (DEBUG) Log.i(TAG, "onUpdateSelection(%d, %d)", selStart, selEnd);
        }
    }

    private class TestAutocompleteEditText extends AutocompleteEditText {
        public TestAutocompleteEditText(Context context, AttributeSet attrs) {
            super(context, attrs);
            if (DEBUG) Log.i(TAG, "TestAutocompleteEditText constructor");
        }

        @Override
        public void onAutocompleteTextStateChanged(boolean updateDisplay) {
            mVerifier.onAutocompleteTextStateChanged(updateDisplay);
        }

        @Override
        public void onUpdateSelectionForTesting(int selStart, int selEnd) {
            mVerifier.onUpdateSelection(selStart, selEnd);
        }
    }

    public AutocompleteEditTextTest() {
        if (DEBUG) ShadowLog.stream = System.out;
    }

    private boolean isUsingSpannableModel() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE);
    }

    @Before
    public void setUp() {
        if (DEBUG) Log.i(TAG, "setUp started.");
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mVerifier = spy(new Verifier());
        mAutocomplete = new TestAutocompleteEditText(mContext, null);
        assertNotNull(mAutocomplete);
        mInOrder = inOrder(mVerifier);
        assertTrue(mAutocomplete.requestFocus());
        assertNotNull(mAutocomplete.onCreateInputConnection(new EditorInfo()));
        mInputConnection = mAutocomplete.getInputConnection();
        assertNotNull(mInputConnection);
        mInOrder.verifyNoMoreInteractions();
        // Feeder should call this at the beginning.
        mAutocomplete.setIgnoreTextChangesForAutocomplete(false);

        if (DEBUG) Log.i(TAG, "setUp finished.");
    }

    private void assertTexts(String userText, String autocompleteText) {
        assertEquals(userText, mAutocomplete.getTextWithoutAutocomplete());
        assertEquals(userText + autocompleteText, mAutocomplete.getTextWithAutocomplete());
        assertEquals(autocompleteText.length(), mAutocomplete.getAutocompleteLength());
        assertEquals(!TextUtils.isEmpty(autocompleteText), mAutocomplete.hasAutocomplete());
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = true))
    public void testAppend_CommitTextWithSpannableModel() {
        internalTestAppend_CommitText();
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = false))
    public void testAppend_CommitTextWithoutSpannableModel() {
        internalTestAppend_CommitText();
    }

    private void internalTestAppend_CommitText() {
        // User types "h".
        assertTrue(mInputConnection.commitText("h", 1));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // The controller kicks in.
        mAutocomplete.setAutocompleteText("h", "ello world");
        // The non-spannable model changes selection in two steps.
        if (isUsingSpannableModel()) {
            assertFalse(mAutocomplete.isCursorVisible());
        } else {
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            mInOrder.verify(mVerifier).onUpdateSelection(1, 11);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types "he".
        assertTrue(mInputConnection.commitText("e", 1));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(2, 2);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        } else {
            mInOrder.verify(mVerifier).onUpdateSelection(2, 11);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("he", "llo world");
        if (isUsingSpannableModel()) assertFalse(mAutocomplete.isCursorVisible());
        mInOrder.verifyNoMoreInteractions();
        assertTexts("he", "llo world");
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types "hello".
        assertTrue(mInputConnection.commitText("llo", 1));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world");
        if (isUsingSpannableModel()) {
            assertFalse(mAutocomplete.isCursorVisible());
        } else {
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 11);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTexts("hello", " world");
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types a space.
        assertTrue(mInputConnection.beginBatchEdit());
        // We should still show the intermediate autocomplete text to the user even in the middle of
        // a batch edit. Otherwise, the user may see flickering of autocomplete text.
        assertEquals("hello world", mAutocomplete.getText().toString());
        assertTrue(mInputConnection.commitText(" ", 1));
        assertEquals("hello world", mAutocomplete.getText().toString());
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertEquals("hello world", mAutocomplete.getText().toString());

        mInOrder.verifyNoMoreInteractions();
        assertTrue(mInputConnection.endBatchEdit());

        // Autocomplete text gets redrawn.
        assertTexts("hello ", "world");
        assertTrue(mAutocomplete.shouldAutocomplete());
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(6, 6);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            mInOrder.verify(mVerifier).onUpdateSelection(6, 11);
        }

        mAutocomplete.setAutocompleteText("hello ", "world");
        if (isUsingSpannableModel()) assertFalse(mAutocomplete.isCursorVisible());
        assertTexts("hello ", "world");
        mInOrder.verifyNoMoreInteractions();
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = true))
    public void testAppend_SetComposingTextWithSpannableModel() {
        internalTestAppend_SetComposingText();
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = false))
    public void testAppend_SetComposingTextWithoutSpannableModel() {
        internalTestAppend_SetComposingText();
    }

    private void internalTestAppend_SetComposingText() {
        // User types "h".
        assertTrue(mInputConnection.setComposingText("h", 1));

        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
        }
        mInOrder.verifyNoMoreInteractions();

        // The old model does not allow autocompletion here.
        assertEquals(isUsingSpannableModel(), mAutocomplete.shouldAutocomplete());
        if (isUsingSpannableModel()) {
            // The controller kicks in.
            mAutocomplete.setAutocompleteText("h", "ello world");
            assertFalse(mAutocomplete.isCursorVisible());
            assertTexts("h", "ello world");
        } else {
            assertTexts("h", "");
        }
        mInOrder.verifyNoMoreInteractions();

        // User types "hello".
        assertTrue(mInputConnection.setComposingText("hello", 1));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        }
        mInOrder.verifyNoMoreInteractions();
        if (isUsingSpannableModel()) {
            assertTexts("hello", " world");
        } else {
            // The old model does not work with composition.
            assertTexts("hello", "");
        }

        // The old model does not allow autocompletion here.
        assertEquals(isUsingSpannableModel(), mAutocomplete.shouldAutocomplete());
        if (isUsingSpannableModel()) {
            // The controller kicks in.
            mAutocomplete.setAutocompleteText("hello", " world");
            assertFalse(mAutocomplete.isCursorVisible());
            assertTexts("hello", " world");
        } else {
            assertTexts("hello", "");
        }
        mInOrder.verifyNoMoreInteractions();

        // User types a space.
        assertTrue(mInputConnection.beginBatchEdit());
        // We should still show the intermediate autocomplete text to the user even in the middle of
        // a batch edit. Otherwise, the user may see flickering of autocomplete text.
        if (isUsingSpannableModel()) {
            assertEquals("hello world", mAutocomplete.getText().toString());
        }
        assertTrue(mInputConnection.finishComposingText());
        if (isUsingSpannableModel()) {
            assertEquals("hello world", mAutocomplete.getText().toString());
        }
        assertTrue(mInputConnection.commitText(" ", 1));
        if (isUsingSpannableModel()) {
            assertEquals("hello world", mAutocomplete.getText().toString());
        }

        mInOrder.verifyNoMoreInteractions();
        assertTrue(mInputConnection.endBatchEdit());
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(6, 6);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            mInOrder.verify(mVerifier).onUpdateSelection(6, 6);
        }
        mInOrder.verifyNoMoreInteractions();

        if (isUsingSpannableModel()) {
            // Autocomplete text has been drawn at endBatchEdit().
            assertTexts("hello ", "world");
        } else {
            assertTexts("hello ", "");
        }

        // The old model can also autocomplete now.
        assertTrue(mAutocomplete.shouldAutocomplete());
        mAutocomplete.setAutocompleteText("hello ", "world");
        assertTexts("hello ", "world");
        if (isUsingSpannableModel()) {
            assertFalse(mAutocomplete.isCursorVisible());
        } else {
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            mInOrder.verify(mVerifier).onUpdateSelection(6, 11);
        }
        mInOrder.verifyNoMoreInteractions();
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = true))
    public void testAppend_DispatchKeyEventWithSpannableModel() {
        internalTestAppend_DispatchKeyEvent();
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = false))
    public void testAppend_DispatchKeyEventWithoutSpannableModel() {
        internalTestAppend_DispatchKeyEvent();
    }

    private void internalTestAppend_DispatchKeyEvent() {
        // User types "h".
        mAutocomplete.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_H));
        mAutocomplete.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_H));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // The controller kicks in.
        mAutocomplete.setAutocompleteText("h", "ello world");
        // The non-spannable model changes selection in two steps.
        if (isUsingSpannableModel()) {
            assertFalse(mAutocomplete.isCursorVisible());
        } else {
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            mInOrder.verify(mVerifier).onUpdateSelection(1, 11);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types "he".
        mAutocomplete.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_E));
        mAutocomplete.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_E));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(2, 2);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            // The new model tries to reuse autocomplete text.
            assertTexts("he", "llo world");
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            mInOrder.verify(mVerifier).onUpdateSelection(2, 2);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("he", "llo world");
        if (isUsingSpannableModel()) {
            assertFalse(mAutocomplete.isCursorVisible());
        } else {
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            mInOrder.verify(mVerifier).onUpdateSelection(2, 11);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTexts("he", "llo world");
        assertTrue(mAutocomplete.shouldAutocomplete());
        mInOrder.verifyNoMoreInteractions();
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = true))
    public void testDelete_SetComposingTextWithSpannableModel() {
        // User types "hello".
        assertTrue(mInputConnection.setComposingText("hello", 1));

        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world");
        assertFalse(mAutocomplete.isCursorVisible());
        assertTexts("hello", " world");
        mInOrder.verifyNoMoreInteractions();

        // User deletes 'o'.
        assertTrue(mInputConnection.setComposingText("hell", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(4, 4);
        // We restore 'o', and clears autocomplete text instead.
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        assertTrue(mAutocomplete.isCursorVisible());
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello", "");

        // Keyboard app checks the current state.
        assertEquals("hello", mInputConnection.getTextBeforeCursor(10, 0));
        assertTrue(mAutocomplete.isCursorVisible());
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello", "");
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = true))
    public void testSelect_SelectAutocompleteWithSpannableModel() {
        internalTestSelect_SelectAutocomplete();
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = false))
    public void testSelect_SelectAutocompleteWithoutSpannableModel() {
        internalTestSelect_SelectAutocomplete();
    }

    private void internalTestSelect_SelectAutocomplete() {
        // User types "hello".
        assertTrue(mInputConnection.commitText("hello", 1));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world");
        assertTexts("hello", " world");
        if (isUsingSpannableModel()) {
            assertFalse(mAutocomplete.isCursorVisible());
        } else {
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 11);
        }
        mInOrder.verifyNoMoreInteractions();
        // User touches autocomplete text.
        mAutocomplete.setSelection(7);
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(7, 7);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            mInOrder.verify(mVerifier).onUpdateSelection(7, 7);
        }
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello world", "");
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = true))
    public void testSelect_SelectUserTextWithSpannableModel() {
        internalTestSelect_SelectUserText();
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = false))
    public void testSelect_SelectUserTextWithoutSpannableModel() {
        internalTestSelect_SelectUserText();
    }

    private void internalTestSelect_SelectUserText() {
        // User types "hello".
        assertTrue(mInputConnection.commitText("hello", 1));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world");
        assertTexts("hello", " world");
        if (isUsingSpannableModel()) {
            assertFalse(mAutocomplete.isCursorVisible());
        } else {
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 11);
        }
        mInOrder.verifyNoMoreInteractions();
        // User touches the user text.
        mAutocomplete.setSelection(3);
        if (isUsingSpannableModel()) {
            assertTrue(mAutocomplete.isCursorVisible());
            mInOrder.verify(mVerifier).onUpdateSelection(3, 3);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            mInOrder.verify(mVerifier).onUpdateSelection(3, 3);
        }
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        // Autocomplete text is removed.
        assertTexts("hello", "");
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = true))
    public void testAppend_AfterSelectAllWithSpannableModel() {
        internalTestAppend_AfterSelectAll();
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = false))
    public void testAppend_AfterSelectAllWithoutSpannableModel() {
        internalTestAppend_AfterSelectAll();
    }

    private void internalTestAppend_AfterSelectAll() {
        final String url = "https://www.google.com/";
        mAutocomplete.setText(url);
        mAutocomplete.setSelection(0, url.length());
        assertTrue(mAutocomplete.isCursorVisible());
        // User types "h" - note that this is also starting character of the URL. The selection gets
        // replaced by what user types.
        assertTrue(mInputConnection.commitText("h", 1));
        // We want to allow inline autocomplete when the user overrides an existing URL.
        assertTrue(mAutocomplete.shouldAutocomplete());
        assertTrue(mAutocomplete.isCursorVisible());
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = true))
    public void testIgnoreAndGetWithSpannableModel() {
        internalTestIgnoreAndGet();
    }

    @Test
    @Features(@Features.Register(
            value = ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enabled = false))
    public void testIgnoreAndGetWithoutSpannableModel() {
        internalTestIgnoreAndGet();
    }

    private void internalTestIgnoreAndGet() {
        final String url = "https://www.google.com/";
        mAutocomplete.setIgnoreTextChangesForAutocomplete(true);
        mAutocomplete.setText(url);
        mAutocomplete.setIgnoreTextChangesForAutocomplete(false);
        mInputConnection.getTextBeforeCursor(1, 1);
        if (isUsingSpannableModel()) {
            assertTrue(mAutocomplete.isCursorVisible());
        }
        mInOrder.verifyNoMoreInteractions();
    }
}