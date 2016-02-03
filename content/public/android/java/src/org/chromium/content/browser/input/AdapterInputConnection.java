// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.text.Editable;
import android.text.InputType;
import android.text.Selection;
import android.util.StringBuilderPrinter;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.blink_public.web.WebTextInputFlags;
import org.chromium.ui.base.ime.TextInputType;

import java.util.Locale;

/**
 * InputConnection is created by ContentView.onCreateInputConnection.
 * It then adapts android's IME to chrome's RenderWidgetHostView using the
 * native ImeAdapterAndroid via the class ImeAdapter.
 */
public class AdapterInputConnection extends BaseInputConnection {
    private static final String TAG = "cr_Ime";
    private static final boolean DEBUG_LOGS = false;
    /**
     * Selection value should be -1 if not known. See EditorInfo.java for details.
     */
    public static final int INVALID_SELECTION = -1;
    public static final int INVALID_COMPOSITION = -1;

    private final ImeAdapter mImeAdapter;

    private boolean mSingleLine;
    private int mNumNestedBatchEdits = 0;
    private int mPendingAccent;

    private int mLastUpdateSelectionStart = INVALID_SELECTION;
    private int mLastUpdateSelectionEnd = INVALID_SELECTION;
    private int mLastUpdateCompositionStart = INVALID_COMPOSITION;
    private int mLastUpdateCompositionEnd = INVALID_COMPOSITION;

    @VisibleForTesting
    AdapterInputConnection(View view, ImeAdapter imeAdapter, int initialSelStart, int initialSelEnd,
            EditorInfo outAttrs) {
        super(view, true);
        mImeAdapter = imeAdapter;
        mImeAdapter.setInputConnection(this);

        mSingleLine = true;
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN
                | EditorInfo.IME_FLAG_NO_EXTRACT_UI;
        outAttrs.inputType = EditorInfo.TYPE_CLASS_TEXT
                | EditorInfo.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT;

        int inputType = imeAdapter.getTextInputType();
        int inputFlags = imeAdapter.getTextInputFlags();
        if ((inputFlags & WebTextInputFlags.AutocompleteOff) != 0) {
            outAttrs.inputType |= EditorInfo.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
        }

        if (inputType == TextInputType.TEXT) {
            // Normal text field
            outAttrs.imeOptions |= EditorInfo.IME_ACTION_GO;
            if ((inputFlags & WebTextInputFlags.AutocorrectOff) == 0) {
                outAttrs.inputType |= EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT;
            }
        } else if (inputType == TextInputType.TEXT_AREA
                || inputType == TextInputType.CONTENT_EDITABLE) {
            outAttrs.inputType |= EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE;
            if ((inputFlags & WebTextInputFlags.AutocorrectOff) == 0) {
                outAttrs.inputType |= EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT;
            }
            outAttrs.imeOptions |= EditorInfo.IME_ACTION_NONE;
            mSingleLine = false;
        } else if (inputType == TextInputType.PASSWORD) {
            // Password
            outAttrs.inputType = InputType.TYPE_CLASS_TEXT
                    | InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD;
            outAttrs.imeOptions |= EditorInfo.IME_ACTION_GO;
        } else if (inputType == TextInputType.SEARCH) {
            // Search
            outAttrs.imeOptions |= EditorInfo.IME_ACTION_SEARCH;
        } else if (inputType == TextInputType.URL) {
            // Url
            outAttrs.inputType = InputType.TYPE_CLASS_TEXT
                    | InputType.TYPE_TEXT_VARIATION_URI;
            outAttrs.imeOptions |= EditorInfo.IME_ACTION_GO;
        } else if (inputType == TextInputType.EMAIL) {
            // Email
            outAttrs.inputType = InputType.TYPE_CLASS_TEXT
                    | InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS;
            outAttrs.imeOptions |= EditorInfo.IME_ACTION_GO;
        } else if (inputType == TextInputType.TELEPHONE) {
            // Telephone
            // Number and telephone do not have both a Tab key and an
            // action in default OSK, so set the action to NEXT
            outAttrs.inputType = InputType.TYPE_CLASS_PHONE;
            outAttrs.imeOptions |= EditorInfo.IME_ACTION_NEXT;
        } else if (inputType == TextInputType.NUMBER) {
            // Number
            outAttrs.inputType = InputType.TYPE_CLASS_NUMBER
                    | InputType.TYPE_NUMBER_VARIATION_NORMAL
                    | InputType.TYPE_NUMBER_FLAG_DECIMAL;
            outAttrs.imeOptions |= EditorInfo.IME_ACTION_NEXT;
        }

        // Handling of autocapitalize. Blink will send the flag taking into account the element's
        // type. This is not using AutocapitalizeNone because Android does not autocapitalize by
        // default and there is no way to express no capitalization.
        // Autocapitalize is meant as a hint to the virtual keyboard.
        if ((inputFlags & WebTextInputFlags.AutocapitalizeCharacters) != 0) {
            outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS;
        } else if ((inputFlags & WebTextInputFlags.AutocapitalizeWords) != 0) {
            outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_CAP_WORDS;
        } else if ((inputFlags & WebTextInputFlags.AutocapitalizeSentences) != 0) {
            outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_CAP_SENTENCES;
        }
        // Content editable doesn't use autocapitalize so we need to set it manually.
        if (inputType == TextInputType.CONTENT_EDITABLE) {
            outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_CAP_SENTENCES;
        }

        outAttrs.initialSelStart = initialSelStart;
        outAttrs.initialSelEnd = initialSelEnd;
        mLastUpdateSelectionStart = outAttrs.initialSelStart;
        mLastUpdateSelectionEnd = outAttrs.initialSelEnd;
        if (DEBUG_LOGS) {
            Log.w(TAG, "Constructor called with outAttrs: %s", dumpEditorInfo(outAttrs));
        }
    }

    private static String dumpEditorInfo(EditorInfo editorInfo) {
        StringBuilder builder = new StringBuilder();
        StringBuilderPrinter printer = new StringBuilderPrinter(builder);
        editorInfo.dump(printer, "");
        return builder.toString();
    }

    private static String dumpEditable(Editable editable) {
        return String.format(Locale.US, "Editable {[%s] SEL[%d %d] COM[%d %d]}",
                editable.toString(),
                Selection.getSelectionStart(editable),
                Selection.getSelectionEnd(editable),
                getComposingSpanStart(editable),
                getComposingSpanEnd(editable));
    }

    /**
     * Updates the AdapterInputConnection's internal representation of the text being edited and
     * its selection and composition properties. The resulting Editable is accessible through the
     * getEditable() method. If the text has not changed, this also calls updateSelection on the
     * InputMethodManager.
     *
     * @param text The String contents of the field being edited.
     * @param selectionStart The character offset of the selection start, or the caret position if
     *                       there is no selection.
     * @param selectionEnd The character offset of the selection end, or the caret position if there
     *                     is no selection.
     * @param compositionStart The character offset of the composition start, or -1 if there is no
     *                         composition.
     * @param compositionEnd The character offset of the composition end, or -1 if there is no
     *                       selection.
     * @param isNonImeChange True when the update was caused by non-IME (e.g. Javascript).
     */
    @VisibleForTesting
    public void updateState(String text, int selectionStart, int selectionEnd, int compositionStart,
            int compositionEnd, boolean isNonImeChange) {
        if (DEBUG_LOGS) Log.w(TAG, "updateState [%s] [%s %s] [%s %s] [%b]", text, selectionStart,
                selectionEnd, compositionStart, compositionEnd, isNonImeChange);
        // If this update is from the IME, no further state modification is necessary because the
        // state should have been updated already by the IM framework directly.
        if (!isNonImeChange) return;

        // Non-breaking spaces can cause the IME to get confused. Replace with normal spaces.
        text = text.replace('\u00A0', ' ');

        selectionStart = Math.min(selectionStart, text.length());
        selectionEnd = Math.min(selectionEnd, text.length());
        compositionStart = Math.min(compositionStart, text.length());
        compositionEnd = Math.min(compositionEnd, text.length());

        Editable editable = getEditableInternal();

        String prevText = editable.toString();
        boolean textUnchanged = prevText.equals(text);

        if (!textUnchanged) {
            editable.replace(0, editable.length(), text);
        }

        Selection.setSelection(editable, selectionStart, selectionEnd);

        if (compositionStart == compositionEnd) {
            removeComposingSpans(editable);
        } else {
            super.setComposingRegion(compositionStart, compositionEnd);
        }
        updateSelectionIfRequired();
    }

    /**
     * @see BaseInputConnection#getEditable()
     */
    @Override
    public Editable getEditable() {
        Editable editable = getEditableInternal();
        if (DEBUG_LOGS) Log.w(TAG, "getEditable: %s", dumpEditable(editable));
        return editable;
    }

    private Editable getEditableInternal() {
        return mImeAdapter.getEditable();
    }

    /**
     * Sends selection update to the InputMethodManager unless we are currently in a batch edit or
     * if the exact same selection and composition update was sent already.
     */
    private void updateSelectionIfRequired() {
        if (mNumNestedBatchEdits != 0) return;
        Editable editable = getEditableInternal();
        int selectionStart = Selection.getSelectionStart(editable);
        int selectionEnd = Selection.getSelectionEnd(editable);
        int compositionStart = getComposingSpanStart(editable);
        int compositionEnd = getComposingSpanEnd(editable);
        // Avoid sending update if we sent an exact update already previously.
        if (mLastUpdateSelectionStart == selectionStart
                && mLastUpdateSelectionEnd == selectionEnd
                && mLastUpdateCompositionStart == compositionStart
                && mLastUpdateCompositionEnd == compositionEnd) {
            return;
        }
        if (DEBUG_LOGS) Log.w(TAG, "updateSelectionIfRequired [%d %d] [%d %d]", selectionStart,
            selectionEnd, compositionStart, compositionEnd);
        // updateSelection should be called every time the selection or composition changes
        // if it happens not within a batch edit, or at the end of each top level batch edit.
        mImeAdapter.updateSelection(selectionStart, selectionEnd, compositionStart, compositionEnd);
        mLastUpdateSelectionStart = selectionStart;
        mLastUpdateSelectionEnd = selectionEnd;
        mLastUpdateCompositionStart = compositionStart;
        mLastUpdateCompositionEnd = compositionEnd;
    }

    /**
     * @see BaseInputConnection#setComposingText(java.lang.CharSequence, int)
     */
    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        if (DEBUG_LOGS) Log.w(TAG, "setComposingText [%s] [%d]", text, newCursorPosition);
        mPendingAccent = 0;
        super.setComposingText(text, newCursorPosition);
        updateSelectionIfRequired();
        return mImeAdapter.sendCompositionToNative(text, newCursorPosition, false);
    }

    /**
     * @see BaseInputConnection#commitText(java.lang.CharSequence, int)
     */
    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        if (DEBUG_LOGS) Log.w(TAG, "commitText [%s] [%d]", text, newCursorPosition);
        mPendingAccent = 0;
        super.commitText(text, newCursorPosition);
        updateSelectionIfRequired();
        return mImeAdapter.sendCompositionToNative(text, newCursorPosition, text.length() > 0);
    }

    /**
     * @see BaseInputConnection#performEditorAction(int)
     */
    @Override
    public boolean performEditorAction(int actionCode) {
        if (DEBUG_LOGS) Log.w(TAG, "performEditorAction [%d]", actionCode);
        return mImeAdapter.performEditorAction(actionCode);
    }

    /**
     * @see BaseInputConnection#performContextMenuAction(int)
     */
    @Override
    public boolean performContextMenuAction(int id) {
        if (DEBUG_LOGS) Log.w(TAG, "performContextMenuAction [%d]", id);
        return mImeAdapter.performContextMenuAction(id);
    }

    /**
     * @see BaseInputConnection#getExtractedText(android.view.inputmethod.ExtractedTextRequest,
     *                                           int)
     */
    @Override
    public ExtractedText getExtractedText(ExtractedTextRequest request, int flags) {
        if (DEBUG_LOGS) Log.w(TAG, "getExtractedText");
        Editable editable = getEditableInternal();
        ExtractedText et = new ExtractedText();
        et.text = editable.toString();
        et.partialEndOffset = editable.length();
        et.selectionStart = Selection.getSelectionStart(editable);
        et.selectionEnd = Selection.getSelectionEnd(editable);
        et.flags = mSingleLine ? ExtractedText.FLAG_SINGLE_LINE : 0;
        return et;
    }

    /**
     * @see BaseInputConnection#beginBatchEdit()
     */
    @Override
    public boolean beginBatchEdit() {
        if (DEBUG_LOGS) Log.w(TAG, "beginBatchEdit [%b]", (mNumNestedBatchEdits == 0));
        mNumNestedBatchEdits++;
        return true;
    }

    /**
     * @see BaseInputConnection#endBatchEdit()
     */
    @Override
    public boolean endBatchEdit() {
        if (mNumNestedBatchEdits == 0) return false;
        --mNumNestedBatchEdits;
        if (DEBUG_LOGS) Log.w(TAG, "endBatchEdit [%b]", (mNumNestedBatchEdits == 0));
        if (mNumNestedBatchEdits == 0) updateSelectionIfRequired();
        return mNumNestedBatchEdits != 0;
    }

    /**
     * @see BaseInputConnection#deleteSurroundingText(int, int)
     */
    @Override
    public boolean deleteSurroundingText(int beforeLength, int afterLength) {
        return deleteSurroundingTextImpl(beforeLength, afterLength, false);
    }

    /**
     * Check if the given {@code index} is between UTF-16 surrogate pair.
     * @param str The String.
     * @param index The index
     * @return True if the index is between UTF-16 surrogate pair, false otherwise.
     */
    @VisibleForTesting
    static boolean isIndexBetweenUtf16SurrogatePair(CharSequence str, int index) {
        return index > 0 && index < str.length() && Character.isHighSurrogate(str.charAt(index - 1))
                && Character.isLowSurrogate(str.charAt(index));
    }

    private boolean deleteSurroundingTextImpl(
            int beforeLength, int afterLength, boolean fromPhysicalKey) {
        if (DEBUG_LOGS) {
            Log.w(TAG, "deleteSurroundingText [%d %d %b]", beforeLength, afterLength,
                    fromPhysicalKey);
        }

        if (mPendingAccent != 0) {
            finishComposingText();
        }

        Editable editable = getEditableInternal();
        int selectionStart = Selection.getSelectionStart(editable);
        int selectionEnd = Selection.getSelectionEnd(editable);
        int availableBefore = selectionStart;
        int availableAfter = editable.length() - selectionEnd;
        beforeLength = Math.min(beforeLength, availableBefore);
        afterLength = Math.min(afterLength, availableAfter);

        // Adjust these values even before calling super.deleteSurroundingText() to be consistent
        // with the super class.
        if (isIndexBetweenUtf16SurrogatePair(editable, selectionStart - beforeLength)) {
            beforeLength += 1;
        }
        if (isIndexBetweenUtf16SurrogatePair(editable, selectionEnd + afterLength)) {
            afterLength += 1;
        }

        super.deleteSurroundingText(beforeLength, afterLength);
        updateSelectionIfRequired();

        // If this was called due to a physical key, no need to generate a key event here as
        // the caller will take care of forwarding the original.
        if (fromPhysicalKey) {
            return true;
        }

        return mImeAdapter.deleteSurroundingText(beforeLength, afterLength);
    }

    /**
     * @see BaseInputConnection#sendKeyEvent(android.view.KeyEvent)
     */
    @Override
    public boolean sendKeyEvent(KeyEvent event) {
        if (DEBUG_LOGS) {
            Log.w(TAG, "sendKeyEvent [%d] [%d] [%d]", event.getAction(), event.getKeyCode(),
                    event.getUnicodeChar());
        }

        int action = event.getAction();
        int keycode = event.getKeyCode();
        int unicodeChar = event.getUnicodeChar();

        // If this isn't a KeyDown event, no need to update composition state; just pass the key
        // event through and return. But note that some keys, such as enter, may actually be
        // handled on ACTION_UP in Blink.
        if (action != KeyEvent.ACTION_DOWN) {
            mImeAdapter.sendKeyEvent(event);
            return true;
        }

        // If this is backspace/del or if the key has a character representation,
        // need to update the underlying Editable (i.e. the local representation of the text
        // being edited).  Some IMEs like Jellybean stock IME and Samsung IME mix in delete
        // KeyPress events instead of calling deleteSurroundingText.
        if (keycode == KeyEvent.KEYCODE_DEL) {
            deleteSurroundingTextImpl(1, 0, true);
        } else if (keycode == KeyEvent.KEYCODE_FORWARD_DEL) {
            deleteSurroundingTextImpl(0, 1, true);
        } else if (keycode == KeyEvent.KEYCODE_ENTER) {
            // Finish text composition when pressing enter, as that may submit a form field.
            // TODO(aurimas): remove this workaround when crbug.com/278584 is fixed.
            finishComposingText();
        } else if ((unicodeChar & KeyCharacterMap.COMBINING_ACCENT) != 0) {
            // Store a pending accent character and make it the current composition.
            int pendingAccent = unicodeChar & KeyCharacterMap.COMBINING_ACCENT_MASK;
            StringBuilder builder = new StringBuilder();
            builder.appendCodePoint(pendingAccent);
            setComposingText(builder.toString(), 1);
            mPendingAccent = pendingAccent;
            return true;
        } else if (mPendingAccent != 0 && unicodeChar != 0) {
            int combined = KeyEvent.getDeadChar(mPendingAccent, unicodeChar);
            if (combined != 0) {
                StringBuilder builder = new StringBuilder();
                builder.appendCodePoint(combined);
                commitText(builder.toString(), 1);
                return true;
            }
            // Noncombinable character; commit the accent character and fall through to sending
            // the key event for the character afterwards.
            finishComposingText();
        }
        replaceSelectionWithUnicodeChar(unicodeChar);
        mImeAdapter.sendKeyEvent(event);
        return true;
    }

    /**
     * Update the Editable to reflect what Blink will do in response to the KeyDown for a
     * unicode-mapped key event.
     * @param unicodeChar The Unicode character to update selection with.
     */
    private void replaceSelectionWithUnicodeChar(int unicodeChar) {
        if (unicodeChar == 0) return;
        Editable editable = getEditableInternal();
        int selectionStart = Selection.getSelectionStart(editable);
        int selectionEnd = Selection.getSelectionEnd(editable);
        if (selectionStart > selectionEnd) {
            int temp = selectionStart;
            selectionStart = selectionEnd;
            selectionEnd = temp;
        }
        editable.replace(selectionStart, selectionEnd, Character.toString((char) unicodeChar));
        updateSelectionIfRequired();
    }

    /**
     * @see BaseInputConnection#finishComposingText()
     */
    @Override
    public boolean finishComposingText() {
        if (DEBUG_LOGS) Log.w(TAG, "finishComposingText");
        mPendingAccent = 0;

        if (getComposingSpanStart(getEditableInternal())
                == getComposingSpanEnd(getEditableInternal())) {
            return true;
        }

        super.finishComposingText();
        updateSelectionIfRequired();
        mImeAdapter.finishComposingText();

        return true;
    }

    /**
     * @see BaseInputConnection#setSelection(int, int)
     */
    @Override
    public boolean setSelection(int start, int end) {
        if (DEBUG_LOGS) Log.w(TAG, "setSelection [%d %d]", start, end);
        int textLength = getEditableInternal().length();
        if (start < 0 || end < 0 || start > textLength || end > textLength) return true;
        super.setSelection(start, end);
        updateSelectionIfRequired();
        return mImeAdapter.setEditableSelectionOffsets(start, end);
    }

    /**
     * Call this when restartInput() is called.
     */
    void onRestartInput() {
        if (DEBUG_LOGS) Log.w(TAG, "onRestartInput");
        mNumNestedBatchEdits = 0;
        mPendingAccent = 0;
    }

    /**
     * @see BaseInputConnection#setComposingRegion(int, int)
     */
    @Override
    public boolean setComposingRegion(int start, int end) {
        if (DEBUG_LOGS) Log.w(TAG, "setComposingRegion [%d %d]", start, end);
        Editable editable = getEditableInternal();
        int textLength = editable.length();
        int a = Math.min(start, end);
        int b = Math.max(start, end);
        if (a < 0) a = 0;
        if (b < 0) b = 0;
        if (a > textLength) a = textLength;
        if (b > textLength) b = textLength;

        if (a == b) {
            removeComposingSpans(editable);
        } else {
            super.setComposingRegion(a, b);
        }
        updateSelectionIfRequired();

        CharSequence regionText = null;
        if (b > a) {
            regionText = editable.subSequence(a, b);
        }
        return mImeAdapter.setComposingRegion(regionText, a, b);
    }

    @VisibleForTesting
    static class ImeState {
        public final String text;
        public final int selectionStart;
        public final int selectionEnd;
        public final int compositionStart;
        public final int compositionEnd;

        public ImeState(String text, int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd) {
            this.text = text;
            this.selectionStart = selectionStart;
            this.selectionEnd = selectionEnd;
            this.compositionStart = compositionStart;
            this.compositionEnd = compositionEnd;
        }
    }

    @VisibleForTesting
    ImeState getImeStateForTesting() {
        Editable editable = getEditableInternal();
        String text = editable.toString();
        int selectionStart = Selection.getSelectionStart(editable);
        int selectionEnd = Selection.getSelectionEnd(editable);
        int compositionStart = getComposingSpanStart(editable);
        int compositionEnd = getComposingSpanEnd(editable);
        return new ImeState(text, selectionStart, selectionEnd, compositionStart, compositionEnd);
    }
}
