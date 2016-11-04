// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.Selection;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;

/**
 * InputConnection is created by ContentView.onCreateInputConnection.
 * It then adapts android's IME to chrome's RenderWidgetHostView using the
 * native ImeAdapterAndroid via the class ImeAdapter. Replica refers to the local copy of
 * the textbox held in the Editable.
 */
public class ReplicaInputConnection
        extends BaseInputConnection implements ChromiumBaseInputConnection {
    private static final String TAG = "cr_Ime";
    private static final boolean DEBUG_LOGS = false;
    /**
     * Selection value should be -1 if not known. See EditorInfo.java for details.
     */
    public static final int INVALID_SELECTION = -1;

    public static final int INVALID_COMPOSITION = -1;

    private final ImeAdapter mImeAdapter;

    // This holds the state of editable text (e.g. contents of <input>, contenteditable) of
    // a focused element.
    // Every time the user, IME, javascript (Blink), autofill etc. modifies the content, the new
    // state must be reflected to this to keep consistency.
    private final Editable mEditable;

    private boolean mSingleLine;
    private int mNumNestedBatchEdits = 0;
    private int mPendingAccent;
    private final Handler mHandler;

    /**
     * Default factory for AdapterInputConnection classes.
     */
    static class Factory implements ChromiumBaseInputConnection.Factory {
        // Note: we share Editable among input connections so that data remains the same on
        // switching inputs. However, the downside is that initial value cannot be correct, and
        // wrong value will be used when jumping from one input to another.
        private final Editable mEditable;

        private final Handler mHandler;

        Factory() {
            mHandler = new Handler(Looper.getMainLooper());
            mEditable = Editable.Factory.getInstance().newEditable("");
            Selection.setSelection(mEditable, 0);
        }

        @Override
        public ReplicaInputConnection initializeAndGet(View view, ImeAdapter imeAdapter,
                int inputType, int inputFlags, int selectionStart, int selectionEnd,
                EditorInfo outAttrs) {
            new InputMethodUma().recordProxyViewReplicaInputConnection();
            return new ReplicaInputConnection(
                    view, imeAdapter, mHandler, mEditable, inputType, inputFlags, outAttrs);
        }

        @Override
        public Handler getHandler() {
            return mHandler;
        }

        @Override
        public void onWindowFocusChanged(boolean gainFocus) {}

        @Override
        public void onViewFocusChanged(boolean gainFocus) {}

        @Override
        public void onViewAttachedToWindow() {}

        @Override
        public void onViewDetachedFromWindow() {}
    }

    @VisibleForTesting
    ReplicaInputConnection(View view, ImeAdapter imeAdapter, Handler handler, Editable editable,
            int inputType, int inputFlags, EditorInfo outAttrs) {
        super(view, true);
        mImeAdapter = imeAdapter;
        mEditable = editable;
        mHandler = handler;

        int initialSelStart = Selection.getSelectionStart(editable);
        int initialSelEnd = Selection.getSelectionEnd(editable);
        ImeUtils.computeEditorInfo(inputType, inputFlags, initialSelStart, initialSelEnd, outAttrs);

        if (DEBUG_LOGS) {
            Log.w(TAG, "Constructor called with outAttrs: %s",
                    ImeUtils.getEditorInfoDebugString(outAttrs));
        }
    }

    @Override
    public void updateStateOnUiThread(String text, int selectionStart, int selectionEnd,
            int compositionStart, int compositionEnd, boolean singleLine, boolean isNonImeChange) {
        if (DEBUG_LOGS) {
            Log.w(TAG, "updateState [%s] [%s %s] [%s %s] [%b] [%b]", text, selectionStart,
                    selectionEnd, compositionStart, compositionEnd, singleLine, isNonImeChange);
        }
        mSingleLine = singleLine;

        // If this update is from the IME, no further state modification is necessary because the
        // state should have been updated already by the IM framework directly.
        if (!isNonImeChange) return;

        // Non-breaking spaces can cause the IME to get confused. Replace with normal spaces.
        text = text.replace('\u00A0', ' ');

        selectionStart = Math.min(selectionStart, text.length());
        selectionEnd = Math.min(selectionEnd, text.length());
        compositionStart = Math.min(compositionStart, text.length());
        compositionEnd = Math.min(compositionEnd, text.length());

        String prevText = mEditable.toString();
        boolean textUnchanged = prevText.equals(text);

        if (!textUnchanged) {
            mEditable.replace(0, mEditable.length(), text);
        }

        Selection.setSelection(mEditable, selectionStart, selectionEnd);

        if (compositionStart == compositionEnd) {
            removeComposingSpans(mEditable);
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
        if (DEBUG_LOGS) Log.w(TAG, "getEditable: %s", ImeUtils.getEditableDebugString(mEditable));
        return mEditable;
    }

    /**
     * Sends selection update to the InputMethodManager unless we are currently in a batch edit or
     * if the exact same selection and composition update was sent already.
     */
    private void updateSelectionIfRequired() {
        if (mNumNestedBatchEdits != 0) return;
        int selectionStart = Selection.getSelectionStart(mEditable);
        int selectionEnd = Selection.getSelectionEnd(mEditable);
        int compositionStart = getComposingSpanStart(mEditable);
        int compositionEnd = getComposingSpanEnd(mEditable);
        if (DEBUG_LOGS) {
            Log.w(TAG, "updateSelectionIfRequired [%d %d] [%d %d]", selectionStart, selectionEnd,
                    compositionStart, compositionEnd);
        }
        // updateSelection should be called every time the selection or composition changes
        // if it happens not within a batch edit, or at the end of each top level batch edit.
        mImeAdapter.updateSelection(selectionStart, selectionEnd, compositionStart, compositionEnd);
    }

    /**
     * @see BaseInputConnection#setComposingText(java.lang.CharSequence, int)
     */
    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        if (DEBUG_LOGS) Log.w(TAG, "setComposingText [%s] [%d]", text, newCursorPosition);
        return updateComposingText(text, newCursorPosition, false);
    }

    /**
     * Sends composing update to the InputMethodManager.
     */
    private boolean updateComposingText(
            final CharSequence text, final int newCursorPosition, final boolean isPendingAccent) {
        final int accentToSend = isPendingAccent ? mPendingAccent : 0;
        mPendingAccent = 0;
        super.setComposingText(text, newCursorPosition);
        updateSelectionIfRequired();
        return mImeAdapter.sendCompositionToNative(text, newCursorPosition, false, accentToSend);
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
        return mImeAdapter.sendCompositionToNative(text, newCursorPosition, true, 0);
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
        ExtractedText et = new ExtractedText();
        et.text = mEditable.toString();
        et.partialEndOffset = mEditable.length();
        et.selectionStart = Selection.getSelectionStart(mEditable);
        et.selectionEnd = Selection.getSelectionEnd(mEditable);
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

        int selectionStart = Selection.getSelectionStart(mEditable);
        int selectionEnd = Selection.getSelectionEnd(mEditable);
        int availableBefore = selectionStart;
        int availableAfter = mEditable.length() - selectionEnd;
        beforeLength = Math.min(beforeLength, availableBefore);
        afterLength = Math.min(afterLength, availableAfter);

        // Adjust these values even before calling super.deleteSurroundingText() to be consistent
        // with the super class.
        if (isIndexBetweenUtf16SurrogatePair(mEditable, selectionStart - beforeLength)) {
            beforeLength += 1;
        }
        if (isIndexBetweenUtf16SurrogatePair(mEditable, selectionEnd + afterLength)) {
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
     * @see ChromiumBaseInputConnection#sendKeyEventOnUiThread(KeyEvent)
     */
    @Override
    public boolean sendKeyEventOnUiThread(KeyEvent event) {
        return sendKeyEvent(event);
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
            updateComposingText(builder.toString(), 1, true);
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
        int selectionStart = Selection.getSelectionStart(mEditable);
        int selectionEnd = Selection.getSelectionEnd(mEditable);
        if (selectionStart > selectionEnd) {
            int temp = selectionStart;
            selectionStart = selectionEnd;
            selectionEnd = temp;
        }
        mEditable.replace(selectionStart, selectionEnd, Character.toString((char) unicodeChar));
        updateSelectionIfRequired();
    }

    /**
     * @see BaseInputConnection#finishComposingText()
     */
    @Override
    public boolean finishComposingText() {
        if (DEBUG_LOGS) Log.w(TAG, "finishComposingText");
        mPendingAccent = 0;

        if (getComposingSpanStart(mEditable) == getComposingSpanEnd(mEditable)) {
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
        int textLength = mEditable.length();
        if (start < 0 || end < 0 || start > textLength || end > textLength) return true;
        super.setSelection(start, end);
        updateSelectionIfRequired();
        return mImeAdapter.setEditableSelectionOffsets(start, end);
    }

    /**
     * @see BaseInputConnection#setComposingRegion(int, int)
     */
    @Override
    public boolean setComposingRegion(int start, int end) {
        if (DEBUG_LOGS) Log.w(TAG, "setComposingRegion [%d %d]", start, end);
        int textLength = mEditable.length();
        int a = Math.min(start, end);
        int b = Math.max(start, end);
        if (a < 0) a = 0;
        if (b < 0) b = 0;
        if (a > textLength) a = textLength;
        if (b > textLength) b = textLength;

        if (a == b) {
            removeComposingSpans(mEditable);
        } else {
            super.setComposingRegion(a, b);
        }
        updateSelectionIfRequired();
        return mImeAdapter.setComposingRegion(a, b);
    }

    @Override
    public void onRestartInputOnUiThread() {
        if (DEBUG_LOGS) Log.w(TAG, "onRestartInputOnUiThread");
        mNumNestedBatchEdits = 0;
        mPendingAccent = 0;
    }

    @Override
    public void moveCursorToSelectionEndOnUiThread() {
        if (DEBUG_LOGS) Log.w(TAG, "movecursorToEnd");
        int selectionEnd = Selection.getSelectionEnd(mEditable);
        setSelection(selectionEnd, selectionEnd);
    }

    @Override
    public void unblockOnUiThread() {}

    @Override
    public Handler getHandler() {
        return mHandler;
    }

    /**
     * @see BaseInputConnection#requestCursorUpdates(int)
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    @Override
    public boolean requestCursorUpdates(int cursorUpdateMode) {
        return mImeAdapter.onRequestCursorUpdates(cursorUpdateMode);
    }

    @VisibleForTesting
    static class ImeState {
        public final String text;
        public final int selectionStart;
        public final int selectionEnd;
        public final int compositionStart;
        public final int compositionEnd;

        public ImeState(String text, int selectionStart, int selectionEnd, int compositionStart,
                int compositionEnd) {
            this.text = text;
            this.selectionStart = selectionStart;
            this.selectionEnd = selectionEnd;
            this.compositionStart = compositionStart;
            this.compositionEnd = compositionEnd;
        }
    }

    @VisibleForTesting
    ImeState getImeStateForTesting() {
        String text = mEditable.toString();
        int selectionStart = Selection.getSelectionStart(mEditable);
        int selectionEnd = Selection.getSelectionEnd(mEditable);
        int compositionStart = getComposingSpanStart(mEditable);
        int compositionEnd = getComposingSpanEnd(mEditable);
        return new ImeState(text, selectionStart, selectionEnd, compositionStart, compositionEnd);
    }
}
