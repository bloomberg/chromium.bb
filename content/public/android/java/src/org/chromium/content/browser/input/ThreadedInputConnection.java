// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.CorrectionInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

/**
 * An implementation of {@link InputConnection} to communicate with external input method
 * apps. Note that it is running on IME thread (except for constructor and calls from ImeAdapter)
 * such that it does not block UI thread and returns text values immediately after any change
 * to them.
 */
public class ThreadedInputConnection implements ChromiumBaseInputConnection {
    private static final String TAG = "cr_Ime";
    private static final boolean DEBUG_LOGS = false;

    private static final TextInputState UNBLOCKER = new TextInputState(
            "", new Range(0, 0), new Range(-1, -1), false, false /* notFromIme */) {

        @Override
        public boolean shouldUnblock() {
            return true;
        }
    };

    private final Runnable mProcessPendingInputStatesRunnable = new Runnable() {
        @Override
        public void run() {
            processPendingInputStates();
        }
    };

    private final Runnable mMoveCursorSelectionEndRunnable = new Runnable() {
        @Override
        public void run() {
            TextInputState textInputState = requestAndWaitForTextInputState();
            if (textInputState == null) return;
            Range selection = textInputState.selection();
            setSelection(selection.end(), selection.end());
        }
    };

    private final Runnable mRequestTextInputStateUpdate = new Runnable() {
        @Override
        public void run() {
            boolean result = mImeAdapter.requestTextInputStateUpdate();
            if (!result) unblockOnUiThread();
        }
    };

    private final Runnable mNotifyUserActionRunnable = new Runnable() {
        @Override
        public void run() {
            mImeAdapter.notifyUserAction();
        }
    };

    private final Runnable mFinishComposingTextRunnable = new Runnable() {
        @Override
        public void run() {
            mImeAdapter.finishComposingText();
        }
    };

    private final ImeAdapter mImeAdapter;
    private final Handler mHandler;
    private int mNumNestedBatchEdits;

    // TODO(changwan): check if we can keep a pool of TextInputState to avoid creating
    // a bunch of new objects for each key stroke.
    private final BlockingQueue<TextInputState> mQueue = new LinkedBlockingQueue<>();
    private int mPendingAccent;

    ThreadedInputConnection(ImeAdapter imeAdapter, Handler handler) {
        if (DEBUG_LOGS) Log.w(TAG, "constructor");
        ImeUtils.checkOnUiThread();
        mImeAdapter = imeAdapter;
        mHandler = handler;
    }

    void initializeOutAttrsOnUiThread(int inputType, int inputFlags, int selectionStart,
            int selectionEnd, EditorInfo outAttrs) {
        ImeUtils.checkOnUiThread();
        mNumNestedBatchEdits = 0;
        mPendingAccent = 0;
        ImeUtils.computeEditorInfo(inputType, inputFlags, selectionStart, selectionEnd, outAttrs);
        if (DEBUG_LOGS) {
            Log.w(TAG, "initializeOutAttrs: " + ImeUtils.getEditorInfoDebugString(outAttrs));
        }
    }

    @Override
    public void updateStateOnUiThread(final String text, final int selectionStart,
            final int selectionEnd, final int compositionStart, final int compositionEnd,
            boolean singleLine, final boolean isNonImeChange) {
        ImeUtils.checkOnUiThread();

        final TextInputState newState =
                new TextInputState(text, new Range(selectionStart, selectionEnd),
                        new Range(compositionStart, compositionEnd), singleLine, !isNonImeChange);
        if (DEBUG_LOGS) Log.w(TAG, "updateState: %s", newState);

        addToQueueOnUiThread(newState);
        if (isNonImeChange) {
            mHandler.post(mProcessPendingInputStatesRunnable);
        }
    }

    /**
     * @see ChromiumBaseInputConnection#getHandler()
     */
    @Override
    public Handler getHandler() {
        return mHandler;
    }

    /**
     * @see ChromiumBaseInputConnection#onRestartInputOnUiThread()
     */
    @Override
    public void onRestartInputOnUiThread() {}

    /**
     * @see ChromiumBaseInputConnection#sendKeyEventOnUiThread(KeyEvent)
     */
    @Override
    public boolean sendKeyEventOnUiThread(final KeyEvent event) {
        ImeUtils.checkOnUiThread();
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                sendKeyEvent(event);
            }
        });
        return true;
    }

    /**
     * @see ChromiumBaseInputConnection#moveCursorToSelectionEndOnUiThread()
     */
    @Override
    public void moveCursorToSelectionEndOnUiThread() {
        mHandler.post(mMoveCursorSelectionEndRunnable);
    }

    @Override
    @VisibleForTesting
    public void unblockOnUiThread() {
        if (DEBUG_LOGS) Log.w(TAG, "unblockOnUiThread");
        ImeUtils.checkOnUiThread();
        addToQueueOnUiThread(UNBLOCKER);
        mHandler.post(mProcessPendingInputStatesRunnable);
    }

    private void processPendingInputStates() {
        if (DEBUG_LOGS) Log.w(TAG, "checkQueue");
        assertOnImeThread();
        // Handle all the remaining states in the queue.
        while (true) {
            TextInputState state = mQueue.poll();
            if (state == null) {
                if (DEBUG_LOGS) Log.w(TAG, "checkQueue - finished");
                return;
            }
            // Unblocker was not used. Ignore.
            if (state.shouldUnblock()) {
                if (DEBUG_LOGS) Log.w(TAG, "checkQueue - ignoring one unblocker");
                continue;
            }
            if (DEBUG_LOGS) Log.w(TAG, "checkQueue: " + state);
            ImeUtils.checkCondition(!state.fromIme());
            updateSelection(state);
        }
    }

    private void updateSelection(TextInputState textInputState) {
        if (textInputState == null) return;
        assertOnImeThread();
        if (mNumNestedBatchEdits != 0) return;
        Range selection = textInputState.selection();
        Range composition = textInputState.composition();
        mImeAdapter.updateSelection(
                selection.start(), selection.end(), composition.start(), composition.end());
    }

    private TextInputState requestAndWaitForTextInputState() {
        if (DEBUG_LOGS) Log.w(TAG, "requestAndWaitForTextInputState");
        ThreadUtils.postOnUiThread(mRequestTextInputStateUpdate);
        return blockAndGetStateUpdate();
    }

    private void addToQueueOnUiThread(TextInputState textInputState) {
        ImeUtils.checkOnUiThread();
        try {
            mQueue.put(textInputState);
        } catch (InterruptedException e) {
            Log.e(TAG, "addToQueueOnUiThread interrupted", e);
        }
        if (DEBUG_LOGS) Log.w(TAG, "addToQueueOnUiThread finished: %d", mQueue.size());
    }

    /**
     * @return BlockingQueue for white box unit testing.
     */
    BlockingQueue<TextInputState> getQueueForTest() {
        return mQueue;
    }

    private void assertOnImeThread() {
        ImeUtils.checkCondition(mHandler.getLooper() == Looper.myLooper());
    }

    /**
     * Block until we get the expected state update.
     * @return TextInputState if we get it successfully. null otherwise.
     */
    private TextInputState blockAndGetStateUpdate() {
        if (DEBUG_LOGS) Log.w(TAG, "blockAndGetStateUpdate");
        assertOnImeThread();
        boolean shouldUpdateSelection = false;
        while (true) {
            TextInputState state;
            try {
                state = mQueue.take();
            } catch (InterruptedException e) {
                // This should never happen since IME thread is artificial and is not exposed
                // to other components.
                e.printStackTrace();
                ImeUtils.checkCondition(false);
                return null;
            }
            if (state.shouldUnblock()) {
                if (DEBUG_LOGS) Log.w(TAG, "blockAndGetStateUpdate: unblocked");
                return null;
            } else if (state.fromIme()) {
                if (shouldUpdateSelection) updateSelection(state);
                if (DEBUG_LOGS) Log.w(TAG, "blockAndGetStateUpdate done: %d", mQueue.size());
                return state;
            }
            // Ignore when state is not from IME, but make sure to update state when we handle
            // state from IME.
            shouldUpdateSelection = true;
        }
    }

    private void notifyUserAction() {
        ThreadUtils.postOnUiThread(mNotifyUserActionRunnable);
    }

    /**
     * @see InputConnection#setComposingText(java.lang.CharSequence, int)
     */
    @Override
    public boolean setComposingText(final CharSequence text, final int newCursorPosition) {
        if (DEBUG_LOGS) Log.w(TAG, "setComposingText [%s] [%d]", text, newCursorPosition);
        return updateComposingText(text, newCursorPosition, false);
    }

    /**
     * Sends composing update to the InputMethodManager.
     */
    @VisibleForTesting
    public boolean updateComposingText(
            final CharSequence text, final int newCursorPosition, final boolean isPendingAccent) {
        final int accentToSend =
                isPendingAccent ? (mPendingAccent | KeyCharacterMap.COMBINING_ACCENT) : 0;
        assertOnImeThread();
        cancelCombiningAccent();
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                mImeAdapter.sendCompositionToNative(text, newCursorPosition, false, accentToSend);
            }
        });
        notifyUserAction();
        return true;
    }

    /**
     * @see InputConnection#commitText(java.lang.CharSequence, int)
     */
    @Override
    public boolean commitText(final CharSequence text, final int newCursorPosition) {
        if (DEBUG_LOGS) Log.w(TAG, "commitText [%s] [%d]", text, newCursorPosition);
        assertOnImeThread();
        cancelCombiningAccent();
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                mImeAdapter.sendCompositionToNative(text, newCursorPosition, text.length() > 0, 0);
            }
        });
        notifyUserAction();
        return true;
    }

    /**
     * @see InputConnection#performEditorAction(int)
     */
    @Override
    public boolean performEditorAction(final int actionCode) {
        if (DEBUG_LOGS) Log.w(TAG, "performEditorAction [%d]", actionCode);
        assertOnImeThread();
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                mImeAdapter.performEditorAction(actionCode);
            }
        });
        return true;
    }

    /**
     * @see InputConnection#performContextMenuAction(int)
     */
    @Override
    public boolean performContextMenuAction(final int id) {
        if (DEBUG_LOGS) Log.w(TAG, "performContextMenuAction [%d]", id);
        assertOnImeThread();
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                mImeAdapter.performContextMenuAction(id);
            }
        });
        return true;
    }

    /**
     * @see InputConnection#getExtractedText(android.view.inputmethod.ExtractedTextRequest, int)
     */
    @Override
    public ExtractedText getExtractedText(ExtractedTextRequest request, int flags) {
        if (DEBUG_LOGS) Log.w(TAG, "getExtractedText");
        assertOnImeThread();
        TextInputState textInputState = requestAndWaitForTextInputState();
        if (textInputState == null) return null;
        ExtractedText extractedText = new ExtractedText();
        extractedText.text = textInputState.text();
        extractedText.partialEndOffset = textInputState.text().length();
        extractedText.selectionStart = textInputState.selection().start();
        extractedText.selectionEnd = textInputState.selection().end();
        extractedText.flags = textInputState.singleLine() ? ExtractedText.FLAG_SINGLE_LINE : 0;
        return extractedText;
    }

    /**
     * @see InputConnection#beginBatchEdit()
     */
    @Override
    public boolean beginBatchEdit() {
        if (DEBUG_LOGS) Log.w(TAG, "beginBatchEdit [%b]", (mNumNestedBatchEdits == 0));
        assertOnImeThread();
        mNumNestedBatchEdits++;
        return true;
    }

    /**
     * @see InputConnection#endBatchEdit()
     */
    @Override
    public boolean endBatchEdit() {
        assertOnImeThread();
        if (mNumNestedBatchEdits == 0) return false;
        --mNumNestedBatchEdits;
        if (DEBUG_LOGS) Log.w(TAG, "endBatchEdit [%b]", (mNumNestedBatchEdits == 0));
        if (mNumNestedBatchEdits == 0) {
            updateSelection(requestAndWaitForTextInputState());
        }
        return mNumNestedBatchEdits != 0;
    }

    /**
     * @see InputConnection#deleteSurroundingText(int, int)
     */
    @Override
    public boolean deleteSurroundingText(final int beforeLength, final int afterLength) {
        if (DEBUG_LOGS) Log.w(TAG, "deleteSurroundingText [%d %d]", beforeLength, afterLength);
        assertOnImeThread();
        if (mPendingAccent != 0) {
            finishComposingText();
        }
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                mImeAdapter.deleteSurroundingText(beforeLength, afterLength);
            }
        });
        return true;
    }

    /**
     * @see InputConnection#deleteSurroundingTextInCodePoints(int, int)
     */
    public boolean deleteSurroundingTextInCodePoints(int beforeLength, int afterLength) {
        // TODO(changwan): Implement this. http://crbug.com/595525
        return false;
    }

    /**
     * @see InputConnection#sendKeyEvent(android.view.KeyEvent)
     */
    @Override
    public boolean sendKeyEvent(final KeyEvent event) {
        if (DEBUG_LOGS) Log.w(TAG, "sendKeyEvent [%d %d]", event.getAction(), event.getKeyCode());
        assertOnImeThread();

        if (handleCombiningAccent(event)) return true;

        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                mImeAdapter.sendKeyEvent(event);
            }
        });
        notifyUserAction();
        return true;
    }

    private boolean handleCombiningAccent(final KeyEvent event) {
        // TODO(changwan): this will break the current composition. check if we can
        // implement it in the renderer instead.
        int action = event.getAction();
        int unicodeChar = event.getUnicodeChar();

        if (action != KeyEvent.ACTION_DOWN) return false;
        if ((unicodeChar & KeyCharacterMap.COMBINING_ACCENT) != 0) {
            int pendingAccent = unicodeChar & KeyCharacterMap.COMBINING_ACCENT_MASK;
            StringBuilder builder = new StringBuilder();
            builder.appendCodePoint(pendingAccent);
            updateComposingText(builder.toString(), 1, true);
            setCombiningAccent(pendingAccent);
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
        return false;
    }

    @VisibleForTesting
    public void setCombiningAccent(int pendingAccent) {
        mPendingAccent = pendingAccent;
    }

    private void cancelCombiningAccent() {
        mPendingAccent = 0;
    }

    /**
     * @see InputConnection#finishComposingText()
     */
    @Override
    public boolean finishComposingText() {
        if (DEBUG_LOGS) Log.w(TAG, "finishComposingText");
        cancelCombiningAccent();
        // This is the only function that may be called on UI thread because
        // of direct calls from InputMethodManager.
        ThreadUtils.postOnUiThread(mFinishComposingTextRunnable);
        return true;
    }

    /**
     * @see InputConnection#setSelection(int, int)
     */
    @Override
    public boolean setSelection(final int start, final int end) {
        if (DEBUG_LOGS) Log.w(TAG, "setSelection [%d %d]", start, end);
        assertOnImeThread();
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                mImeAdapter.setEditableSelectionOffsets(start, end);
            }
        });
        return true;
    }

    /**
     * @see InputConnection#setComposingRegion(int, int)
     */
    @Override
    public boolean setComposingRegion(final int start, final int end) {
        if (DEBUG_LOGS) Log.w(TAG, "setComposingRegion [%d %d]", start, end);
        assertOnImeThread();
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                mImeAdapter.setComposingRegion(start, end);
            }
        });
        return true;
    }

    /**
     * @see InputConnection#getTextBeforeCursor(int, int)
     */
    @Override
    public CharSequence getTextBeforeCursor(int maxChars, int flags) {
        if (DEBUG_LOGS) Log.w(TAG, "getTextBeforeCursor [%d %x]", maxChars, flags);
        assertOnImeThread();
        TextInputState textInputState = requestAndWaitForTextInputState();
        if (textInputState == null) return null;
        return textInputState.getTextBeforeSelection(maxChars);
    }

    /**
     * @see InputConnection#getTextAfterCursor(int, int)
     */
    @Override
    public CharSequence getTextAfterCursor(int maxChars, int flags) {
        if (DEBUG_LOGS) Log.w(TAG, "getTextAfterCursor [%d %x]", maxChars, flags);
        assertOnImeThread();
        TextInputState textInputState = requestAndWaitForTextInputState();
        if (textInputState == null) return null;
        return textInputState.getTextAfterSelection(maxChars);
    }

    /**
     * @see InputConnection#getSelectedText(int)
     */
    @Override
    public CharSequence getSelectedText(int flags) {
        if (DEBUG_LOGS) Log.w(TAG, "getSelectedText [%x]", flags);
        assertOnImeThread();
        TextInputState textInputState = requestAndWaitForTextInputState();
        if (textInputState == null) return null;
        return textInputState.getSelectedText();
    }

    /**
     * @see InputConnection#getCursorCapsMode(int)
     */
    @Override
    public int getCursorCapsMode(int reqModes) {
        if (DEBUG_LOGS) Log.w(TAG, "getCursorCapsMode [%x]", reqModes);
        assertOnImeThread();
        // TODO(changwan): implement this.
        return 0;
    }

    /**
     * @see InputConnection#commitCompletion(android.view.inputmethod.CompletionInfo)
     */
    @Override
    public boolean commitCompletion(CompletionInfo text) {
        if (DEBUG_LOGS) Log.w(TAG, "commitCompletion [%s]", text);
        assertOnImeThread();
        return false;
    }

    /**
     * @see InputConnection#commitCorrection(android.view.inputmethod.CorrectionInfo)
     */
    @Override
    public boolean commitCorrection(CorrectionInfo correctionInfo) {
        if (DEBUG_LOGS) {
            Log.w(TAG, "commitCorrection [%s]",
                    ImeUtils.getCorrectionInfoDebugString(correctionInfo));
        }
        assertOnImeThread();
        return false;
    }

    /**
     * @see InputConnection#clearMetaKeyStates(int)
     */
    @Override
    public boolean clearMetaKeyStates(int states) {
        if (DEBUG_LOGS) Log.w(TAG, "clearMetaKeyStates [%x]", states);
        assertOnImeThread();
        return false;
    }

    /**
     * @see InputConnection#reportFullscreenMode(boolean)
     */
    @Override
    public boolean reportFullscreenMode(boolean enabled) {
        if (DEBUG_LOGS) Log.w(TAG, "reportFullscreenMode [%b]", enabled);
        // We ignore fullscreen mode for now. That's why we set
        // EditorInfo.IME_FLAG_NO_FULLSCREEN in constructor.
        // Note that this may be called on UI thread.
        return false;
    }

    /**
     * @see InputConnection#performPrivateCommand(java.lang.String, android.os.Bundle)
     */
    @Override
    public boolean performPrivateCommand(String action, Bundle data) {
        if (DEBUG_LOGS) Log.w(TAG, "performPrivateCommand [%s]", action);
        assertOnImeThread();
        return false;
    }

    /**
     * @see InputConnection#requestCursorUpdates(int)
     */
    @Override
    public boolean requestCursorUpdates(final int cursorUpdateMode) {
        if (DEBUG_LOGS) Log.w(TAG, "requestCursorUpdates [%x]", cursorUpdateMode);
        assertOnImeThread();
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                mImeAdapter.onRequestCursorUpdates(cursorUpdateMode);
            }
        });
        return true;
    }

    /**
     * @see InputConnection#closeConnection()
     */
    public void closeConnection() {
        // TODO(changwan): Implement this. http://crbug.com/595525
    }
}
