// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Handler;
import android.os.ResultReceiver;
import android.text.Editable;
import android.text.InputType;
import android.text.Selection;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputMethodManager;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 We have to adapt and plumb android IME service and chrome text input API.
 ImeAdapter provides an interface in both ways native <-> java:
 1. InputConnectionAdapter notifies native code of text composition state and
    dispatch key events from java -> WebKit.
 2. Native ImeAdapter notifies java side to clear composition text.

 The basic flow is:
 1. Intercept dispatchKeyEventPreIme() to record the current key event, but do
    nothing else.
 2. When InputConnectionAdapter gets called with composition or result text:
    a) If a key event has been recorded in dispatchKeyEventPreIme() and we
       receive a result text with single character, then we probably need to
       send the result text as a Char event rather than a ConfirmComposition
       event. So we need to dispatch the recorded key event followed by a
       synthetic Char event.
    b) If we receive a composition text or a result text with more than one
       characters, then no matter if we recorded a key event or not in
       dispatchKeyEventPreIme(), we just need to dispatch a synthetic key
       event with special keycode 229, and then dispatch the composition or
       result text.
 3. Intercept dispatchKeyEvent() method for key events not handled by IME, we
    need to dispatch them to WebKit and check webkit's reply. Then inject a
    new key event for further processing if WebKit didn't handle it.
*/
@JNINamespace("content")
class ImeAdapter {
    interface ViewEmbedder {
        /**
         * @param isFinish whether the event is occurring because input is finished.
         */
        public void onImeEvent(boolean isFinish);
        public void onSetFieldValue();
        public void onDismissInput();
        public View getAttachedView();
        public ResultReceiver getNewShowKeyboardReceiver();
    }

    static final int COMPOSITION_KEY_CODE = 229;

    static int sEventTypeRawKeyDown;
    static int sEventTypeKeyUp;
    static int sEventTypeChar;
    static int sTextInputTypeNone;
    static int sTextInputTypeText;
    static int sTextInputTypeTextArea;
    static int sTextInputTypePassword;
    static int sTextInputTypeSearch;
    static int sTextInputTypeUrl;
    static int sTextInputTypeEmail;
    static int sTextInputTypeTel;
    static int sTextInputTypeNumber;
    static int sTextInputTypeWeek;
    static int sTextInputTypeContentEditable;
    static int sModifierShift;
    static int sModifierAlt;
    static int sModifierCtrl;
    static int sModifierCapsLockOn;
    static int sModifierNumLockOn;

    @CalledByNative
    static void initializeWebInputEvents(int eventTypeRawKeyDown, int eventTypeKeyUp,
            int eventTypeChar, int modifierShift, int modifierAlt, int modifierCtrl,
            int modifierCapsLockOn, int modifierNumLockOn) {
        sEventTypeRawKeyDown = eventTypeRawKeyDown;
        sEventTypeKeyUp = eventTypeKeyUp;
        sEventTypeChar = eventTypeChar;
        sModifierShift = modifierShift;
        sModifierAlt = modifierAlt;
        sModifierCtrl = modifierCtrl;
        sModifierCapsLockOn = modifierCapsLockOn;
        sModifierNumLockOn = modifierNumLockOn;
    }

    @CalledByNative
    static void initializeTextInputTypes(int textInputTypeNone, int textInputTypeText,
            int textInputTypeTextArea, int textInputTypePassword, int textInputTypeSearch,
            int textInputTypeUrl, int textInputTypeEmail, int textInputTypeTel,
            int textInputTypeNumber, int textInputTypeDate, int textInputTypeDateTime,
            int textInputTypeDateTimeLocal, int textInputTypeMonth, int textInputTypeTime,
            int textInputTypeWeek, int textInputTypeContentEditable) {
        sTextInputTypeNone = textInputTypeNone;
        sTextInputTypeText = textInputTypeText;
        sTextInputTypeTextArea = textInputTypeTextArea;
        sTextInputTypePassword = textInputTypePassword;
        sTextInputTypeSearch = textInputTypeSearch;
        sTextInputTypeUrl = textInputTypeUrl;
        sTextInputTypeEmail = textInputTypeEmail;
        sTextInputTypeTel = textInputTypeTel;
        sTextInputTypeNumber = textInputTypeNumber;
        sTextInputTypeWeek = textInputTypeWeek;
        sTextInputTypeContentEditable = textInputTypeContentEditable;
    }

    private int mNativeImeAdapterAndroid;
    private int mTextInputType;
    private int mPreImeEventCount;

    private Context mContext;
    private SelectionHandleController mSelectionHandleController;
    private InsertionHandleController mInsertionHandleController;
    private AdapterInputConnection mInputConnection;
    private ViewEmbedder mViewEmbedder;
    private Handler mHandler;

    private class DelayedDismissInput implements Runnable {
        private int mNativeImeAdapter;

        DelayedDismissInput(int nativeImeAdapter) {
            mNativeImeAdapter = nativeImeAdapter;
        }

        @Override
        public void run() {
            attach(mNativeImeAdapter, sTextInputTypeNone);
            dismissInput(true);
        }
    }

    private DelayedDismissInput mDismissInput = null;

    // Delay introduced to avoid hiding the keyboard if new show requests are received.
    // The time required by the unfocus-focus events triggered by tab has been measured in soju:
    // Mean: 18.633 ms, Standard deviation: 7.9837 ms.
    // The value here should be higher enough to cover these cases, but not too high to avoid
    // letting the user perceiving important delays.
    private static final int INPUT_DISMISS_DELAY = 150;

    ImeAdapter(Context context, SelectionHandleController selectionHandleController,
            InsertionHandleController insertionHandleController, ViewEmbedder embedder) {
        mPreImeEventCount = 0;
        mContext = context;
        mSelectionHandleController = selectionHandleController;
        mInsertionHandleController = insertionHandleController;
        mViewEmbedder = embedder;
        mHandler = new Handler();
    }

    boolean isFor(int nativeImeAdapter, int textInputType) {
        return mNativeImeAdapterAndroid == nativeImeAdapter &&
               mTextInputType == textInputType;
    }

    void attachAndShowIfNeeded(int nativeImeAdapter, int textInputType,
            String text, boolean showIfNeeded) {
        mHandler.removeCallbacks(mDismissInput);

        // If current input type is none and showIfNeeded is false, IME should not be shown
        // and input type should remain as none.
        if (mTextInputType == sTextInputTypeNone && !showIfNeeded) {
            return;
        }

        if (!isFor(nativeImeAdapter, textInputType)) {
            // Set a delayed task to perform unfocus. This avoids hiding the keyboard when tabbing
            // through text inputs or when JS rapidly changes focus to another text element.
            if (textInputType == sTextInputTypeNone) {
                mDismissInput = new DelayedDismissInput(nativeImeAdapter);
                mHandler.postDelayed(mDismissInput, INPUT_DISMISS_DELAY);
                return;
            }

            int previousType = mTextInputType;
            attach(nativeImeAdapter, textInputType);

            InputMethodManager manager = (InputMethodManager)
                    mContext.getSystemService(Context.INPUT_METHOD_SERVICE);

            manager.restartInput(mViewEmbedder.getAttachedView());
            if (showIfNeeded) {
                showKeyboard();
            }
        } else if (hasInputType()) {
            showKeyboard();
        }
    }

    void attach(int nativeImeAdapter, int textInputType) {
        mNativeImeAdapterAndroid = nativeImeAdapter;
        mTextInputType = textInputType;
        nativeAttachImeAdapter(mNativeImeAdapterAndroid);
    }

    /**
     * Attaches the imeAdapter to its native counterpart. This is needed to start forwarding
     * keyboard events to WebKit.
     * @param nativeImeAdapter The pointer to the native ImeAdapter object.
     */
    void attach(int nativeImeAdapter) {
        mNativeImeAdapterAndroid = nativeImeAdapter;
        if (nativeImeAdapter != 0) {
            nativeAttachImeAdapter(mNativeImeAdapterAndroid);
        }
    }

    /**
     * Used to check whether the native counterpart of the ImeAdapter has been attached yet.
     * @return Whether native ImeAdapter has been attached and its pointer is currently nonzero.
     */
    boolean isNativeImeAdapterAttached() {
        return mNativeImeAdapterAndroid != 0;
    }

    private void showKeyboard() {
        InputMethodManager manager = (InputMethodManager)
                mContext.getSystemService(Context.INPUT_METHOD_SERVICE);
        manager.showSoftInput(mViewEmbedder.getAttachedView(), 0,
                mViewEmbedder.getNewShowKeyboardReceiver());
    }

    private void dismissInput(boolean unzoomIfNeeded) {
        hideKeyboard(unzoomIfNeeded);
        mViewEmbedder.onDismissInput();
    }

    private void hideKeyboard(boolean unzoomIfNeeded) {
        InputMethodManager manager = (InputMethodManager)
                mContext.getSystemService(Context.INPUT_METHOD_SERVICE);
        View view = mViewEmbedder.getAttachedView();
        if (manager.isActive(view)) {
            manager.hideSoftInputFromWindow(view.getWindowToken(), 0,
                    unzoomIfNeeded ? mViewEmbedder.getNewShowKeyboardReceiver() : null);
        }
    }

    @CalledByNative
    void detach() {
        mNativeImeAdapterAndroid = 0;
        mTextInputType = 0;
    }

    boolean hasInputType() {
        return mTextInputType != sTextInputTypeNone;
    }

    static boolean isTextInputType(int type) {
        return type != sTextInputTypeNone && !InputDialogContainer.isDialogInputType(type);
    }

    boolean hasTextInputType() {
        return isTextInputType(mTextInputType);
    }

    void dispatchKeyEventPreIme(KeyEvent event) {
        // We only register that a key was pressed, but we don't actually intercept
        // it.
        ++mPreImeEventCount;
    }

    boolean dispatchKeyEvent(KeyEvent event) {
        mPreImeEventCount = 0;
        return translateAndSendNativeEvents(event);
    }

    void commitText() {
        cancelComposition();
        if (mNativeImeAdapterAndroid != 0) {
            nativeCommitText(mNativeImeAdapterAndroid, "");
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void cancelComposition() {
        if (mInputConnection != null) {
            mInputConnection.restartInput();
        }
    }

    private boolean checkCompositionQueueAndCallNative(String text, int newCursorPosition,
            boolean isCommit) {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }

        // Committing an empty string finishes the current composition.
        boolean isFinish = text.isEmpty();
        if (!isFinish) {
            mSelectionHandleController.hideAndDisallowAutomaticShowing();
            mInsertionHandleController.hideAndDisallowAutomaticShowing();
        }
        mViewEmbedder.onImeEvent(isFinish);
        boolean hasSingleChar = mPreImeEventCount == 1 && text.length() == 1;
        int keyCode = hasSingleChar ? text.codePointAt(0) : COMPOSITION_KEY_CODE;
        int keyChar = hasSingleChar ? text.codePointAt(0) : 0;
        long timeStampMs = System.currentTimeMillis();
        nativeSendSyntheticKeyEvent(mNativeImeAdapterAndroid, sEventTypeRawKeyDown,
                timeStampMs, keyCode,  keyChar);
        if (hasSingleChar) {
            nativeSendSyntheticKeyEvent(mNativeImeAdapterAndroid, sEventTypeChar,
                    timeStampMs, text.codePointAt(0), text.codePointAt(0));
        } else {
            if (isCommit) {
                nativeCommitText(mNativeImeAdapterAndroid, text);
            } else {
                nativeSetComposingText(mNativeImeAdapterAndroid, text, newCursorPosition);
            }
        }
        nativeSendSyntheticKeyEvent(mNativeImeAdapterAndroid, sEventTypeKeyUp,
                timeStampMs, keyCode, keyChar);
        mPreImeEventCount = 0;
        return true;
    }

    private boolean translateAndSendNativeEvents(KeyEvent event) {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        int action = event.getAction();
        if (action != KeyEvent.ACTION_DOWN &&
            action != KeyEvent.ACTION_UP) {
            // action == KeyEvent.ACTION_MULTIPLE
            // TODO(bulach): confirm the actual behavior. Apparently:
            // If event.getKeyCode() == KEYCODE_UNKNOWN, we can send a
            // composition key down (229) followed by a commit text with the
            // string from event.getUnicodeChars().
            // Otherwise, we'd need to send an event with a
            // WebInputEvent::IsAutoRepeat modifier. We also need to verify when
            // we receive ACTION_MULTIPLE: we may receive it after an ACTION_DOWN,
            // and if that's the case, we'll need to review when to send the Char
            // event.
            return false;
        }
        mViewEmbedder.onImeEvent(false);
        return nativeSendKeyEvent(mNativeImeAdapterAndroid, event, event.getAction(),
                getModifiers(event.getMetaState()), event.getEventTime(), event.getKeyCode(),
                                event.isSystem(), event.getUnicodeChar());
    }

    private void setInputConnection(AdapterInputConnection inputConnection) {
        mInputConnection = inputConnection;
    }

    private static int getModifiers(int metaState) {
        int modifiers = 0;
        if ((metaState & KeyEvent.META_SHIFT_ON) != 0) {
          modifiers |= sModifierShift;
        }
        if ((metaState & KeyEvent.META_ALT_ON) != 0) {
          modifiers |= sModifierAlt;
        }
        if ((metaState & KeyEvent.META_CTRL_ON) != 0) {
          modifiers |= sModifierCtrl;
        }
        if ((metaState & KeyEvent.META_CAPS_LOCK_ON) != 0) {
          modifiers |= sModifierCapsLockOn;
        }
        if ((metaState & KeyEvent.META_NUM_LOCK_ON) != 0) {
          modifiers |= sModifierNumLockOn;
        }
        return modifiers;
    }

    boolean isActive() {
        return mInputConnection != null && mInputConnection.isActive();
    }

    private boolean sendSyntheticKeyEvent(
            int eventType, long timestampMs, int keyCode, int unicodeChar) {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        nativeSendSyntheticKeyEvent(
                mNativeImeAdapterAndroid, eventType, timestampMs, keyCode, unicodeChar);
        return true;
    }

    private boolean deleteSurroundingText(int leftLength, int rightLength) {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        nativeDeleteSurroundingText(mNativeImeAdapterAndroid, leftLength, rightLength);
        return true;
    }

    private boolean setEditableSelectionOffsets(int start, int end) {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        nativeSetEditableSelectionOffsets(mNativeImeAdapterAndroid, start, end);
        return true;
    }

    private boolean setComposingRegion(int start, int end) {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        nativeSetComposingRegion(mNativeImeAdapterAndroid, start, end);
        return true;
    }

    boolean unselect() {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        nativeUnselect(mNativeImeAdapterAndroid);
        return true;
    }

    boolean selectAll() {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        nativeSelectAll(mNativeImeAdapterAndroid);
        return true;
    }

    boolean cut() {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        nativeCut(mNativeImeAdapterAndroid);
        return true;
    }

    boolean copy() {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        nativeCopy(mNativeImeAdapterAndroid);
        return true;
    }

    boolean paste() {
        if (mNativeImeAdapterAndroid == 0) {
            return false;
        }
        nativePaste(mNativeImeAdapterAndroid);
        return true;
    }

    // This InputConnection is created by ContentView.onCreateInputConnection.
    // It then adapts android's IME to chrome's RenderWidgetHostView using the
    // native ImeAdapterAndroid via the outer class ImeAdapter.
    static public class AdapterInputConnection extends BaseInputConnection {
        private View mInternalView;
        private ImeAdapter mImeAdapter;
        private boolean mSingleLine;

        // Factory function.
        static public AdapterInputConnection getInstance(View view, ImeAdapter imeAdapter,
                EditorInfo outAttrs) {
            return new AdapterInputConnection(view, imeAdapter, outAttrs);
        }

        /**
         * Updates the AdapterInputConnection's internal representation of the text
         * being edited and its selection and composition properties. The resulting
         * Editable is accessible through the getEditable() method.
         * If the text has not changed, this also calls updateSelection on the InputMethodManager.
         * @param text The String contents of the field being edited
         * @param selectionStart The character offset of the selection start, or the caret
         * position if there is no selection
         * @param selectionEnd The character offset of the selection end, or the caret
         * position if there is no selection
         * @param compositionStart The character offset of the composition start, or -1
         * if there is no composition
         * @param compositionEnd The character offset of the composition end, or -1
         * if there is no selection
         */
        public void setEditableText(String text, int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd) {

            Editable editable = getEditable();

            int prevSelectionStart = Selection.getSelectionStart(editable);
            int prevSelectionEnd = Selection.getSelectionEnd(editable);
            int prevEditableLength = editable.length();
            int prevCompositionStart = getComposingSpanStart(editable);
            int prevCompositionEnd = getComposingSpanEnd(editable);
            String prevText = editable.toString();

            selectionStart = Math.min(selectionStart, text.length());
            selectionEnd = Math.min(selectionEnd, text.length());
            compositionStart = Math.min(compositionStart, text.length());
            compositionEnd = Math.min(compositionEnd, text.length());

            boolean textUnchanged = prevText.equals(text);

            if (textUnchanged
                    && prevSelectionStart == selectionStart && prevSelectionEnd == selectionEnd
                    && prevCompositionStart == compositionStart
                    && prevCompositionEnd == compositionEnd) {
                // Nothing has changed; don't need to do anything
                return;
            }

            if (!textUnchanged) {
                editable.replace(0, editable.length(), text);
            }
            Selection.setSelection(editable, selectionStart, selectionEnd);
            super.setComposingRegion(compositionStart, compositionEnd);

            if (textUnchanged || prevText.equals("")) {
                // updateSelection should be called when a manual selection change occurs.
                // Should not be called if text is being entered else issues can occur
                // e.g. backspace to undo autocorrection will not work with the default OSK.
                getInputMethodManager().updateSelection(mInternalView,
                        selectionStart, selectionEnd, compositionStart, compositionEnd);
            }

            // When WebKit changes the editable field, both the start and the end positions for
            // the composition will be set to -1. In this case we have to call restart input
            // for the IME to update its state.
            if (textUnchanged && compositionStart == -1 && compositionEnd == -1) {
                restartInput();
            }
        }

        @Override
        public boolean setComposingText(CharSequence text, int newCursorPosition) {
            super.setComposingText(text, newCursorPosition);
            return mImeAdapter.checkCompositionQueueAndCallNative(text.toString(),
                    newCursorPosition, false);
        }

        @Override
        public boolean commitText(CharSequence text, int newCursorPosition) {
            super.commitText(text, newCursorPosition);
            return mImeAdapter.checkCompositionQueueAndCallNative(text.toString(),
                    newCursorPosition, text.length() > 0);
        }

        @Override
        public boolean performEditorAction(int actionCode) {
            switch (actionCode) {
                case EditorInfo.IME_ACTION_NEXT:
                    restartInput();
                    // Send TAB key event
                    long timeStampMs = System.currentTimeMillis();
                    mImeAdapter.sendSyntheticKeyEvent(
                            sEventTypeRawKeyDown, timeStampMs, KeyEvent.KEYCODE_TAB, 0);
                    return true;
                case EditorInfo.IME_ACTION_GO:
                case EditorInfo.IME_ACTION_SEARCH:
                    mImeAdapter.dismissInput(true);
                    break;
            }

            return super.performEditorAction(actionCode);
        }

        @Override
        public boolean performContextMenuAction(int id) {
            switch (id) {
                case android.R.id.selectAll:
                    return mImeAdapter.selectAll();
                case android.R.id.cut:
                    return mImeAdapter.cut();
                case android.R.id.copy:
                    return mImeAdapter.copy();
                case android.R.id.paste:
                    return mImeAdapter.paste();
                default:
                    return false;
            }
        }

        @Override
        public ExtractedText getExtractedText(ExtractedTextRequest request, int flags) {
            ExtractedText et = new ExtractedText();
            Editable editable = getEditable();
            et.text = editable.toString();
            et.partialEndOffset = editable.length();
            et.selectionStart = Selection.getSelectionStart(editable);
            et.selectionEnd = Selection.getSelectionEnd(editable);
            et.flags = mSingleLine ? ExtractedText.FLAG_SINGLE_LINE : 0;
            return et;
        }

        @Override
        public boolean deleteSurroundingText(int leftLength, int rightLength) {
            if (!super.deleteSurroundingText(leftLength, rightLength)) {
                return false;
            }
            return mImeAdapter.deleteSurroundingText(leftLength, rightLength);
        }

        @Override
        public boolean sendKeyEvent(KeyEvent event) {
            mImeAdapter.mSelectionHandleController.hideAndDisallowAutomaticShowing();
            mImeAdapter.mInsertionHandleController.hideAndDisallowAutomaticShowing();

            // If this is a key-up, and backspace/del or if the key has a character representation,
            // need to update the underlying Editable (i.e. the local representation of the text
            // being edited).
            if (event.getAction() == KeyEvent.ACTION_UP) {
                if (event.getKeyCode() == KeyEvent.KEYCODE_DEL) {
                    super.deleteSurroundingText(1, 0);
                } else if (event.getKeyCode() == KeyEvent.KEYCODE_FORWARD_DEL) {
                    super.deleteSurroundingText(0, 1);
                } else {
                    int unicodeChar = event.getUnicodeChar();
                    if (unicodeChar != 0) {
                        Editable editable = getEditable();
                        int selectionStart = Selection.getSelectionStart(editable);
                        int selectionEnd = Selection.getSelectionEnd(editable);
                        if (selectionStart > selectionEnd) {
                            int temp = selectionStart;
                            selectionStart = selectionEnd;
                            selectionEnd = temp;
                        }
                        editable.replace(selectionStart, selectionEnd,
                                Character.toString((char)unicodeChar));
                    }
                }
            }
            return super.sendKeyEvent(event);
        }

        @Override
        public boolean finishComposingText() {
            Editable editable = getEditable();
            if (getComposingSpanStart(editable) == getComposingSpanEnd(editable)) {
                return true;
            }
            super.finishComposingText();
            return mImeAdapter.checkCompositionQueueAndCallNative("", 0, true);
        }

        @Override
        public boolean setSelection(int start, int end) {
            if (start < 0 || end < 0) return true;
            super.setSelection(start, end);
            return mImeAdapter.setEditableSelectionOffsets(start, end);
        }

        /**
         * Informs the InputMethodManager and InputMethodSession (i.e. the IME) that the text
         * state is no longer what the IME has and that it needs to be updated.
         */
        void restartInput() {
            getInputMethodManager().restartInput(mInternalView);
        }

        @Override
        public boolean setComposingRegion(int start, int end) {
            int a = Math.min(start, end);
            int b = Math.max(start, end);
            super.setComposingRegion(a, b);
            return mImeAdapter.setComposingRegion(a, b);
        }

        boolean isActive() {
            return getInputMethodManager().isActive();
        }

        private InputMethodManager getInputMethodManager() {
            return (InputMethodManager) mInternalView.getContext()
                    .getSystemService(Context.INPUT_METHOD_SERVICE);
        }

        private AdapterInputConnection(View view, ImeAdapter imeAdapter, EditorInfo outAttrs) {
            super(view, true);
            mInternalView = view;
            mImeAdapter = imeAdapter;
            mImeAdapter.setInputConnection(this);
            mSingleLine = true;
            outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN;
            outAttrs.inputType = EditorInfo.TYPE_CLASS_TEXT
                    | EditorInfo.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT;
            if (imeAdapter.mTextInputType == ImeAdapter.sTextInputTypeText) {
                // Normal text field
                outAttrs.imeOptions |= EditorInfo.IME_ACTION_GO;
            } else if (imeAdapter.mTextInputType == ImeAdapter.sTextInputTypeTextArea ||
                    imeAdapter.mTextInputType == ImeAdapter.sTextInputTypeContentEditable) {
                // TextArea or contenteditable.
                outAttrs.inputType |= EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE
                        | EditorInfo.TYPE_TEXT_FLAG_CAP_SENTENCES
                        | EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT;
                outAttrs.imeOptions |= EditorInfo.IME_ACTION_NONE;
                mSingleLine = false;
            } else if (imeAdapter.mTextInputType == ImeAdapter.sTextInputTypePassword) {
                // Password
                outAttrs.inputType = InputType.TYPE_CLASS_TEXT
                        | InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD;
                outAttrs.imeOptions |= EditorInfo.IME_ACTION_GO;
            } else if (imeAdapter.mTextInputType == ImeAdapter.sTextInputTypeSearch) {
                // Search
                outAttrs.imeOptions |= EditorInfo.IME_ACTION_SEARCH;
            } else if (imeAdapter.mTextInputType == ImeAdapter.sTextInputTypeUrl) {
                // Url
                // TYPE_TEXT_VARIATION_URI prevents Tab key from showing, so
                // exclude it for now.
                outAttrs.imeOptions |= EditorInfo.IME_ACTION_GO;
            } else if (imeAdapter.mTextInputType == ImeAdapter.sTextInputTypeEmail) {
                // Email
                outAttrs.inputType = InputType.TYPE_CLASS_TEXT
                        | InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS;
                outAttrs.imeOptions |= EditorInfo.IME_ACTION_GO;
            } else if (imeAdapter.mTextInputType == ImeAdapter.sTextInputTypeTel) {
                // Telephone
                // Number and telephone do not have both a Tab key and an
                // action in default OSK, so set the action to NEXT
                outAttrs.inputType = InputType.TYPE_CLASS_PHONE;
                outAttrs.imeOptions |= EditorInfo.IME_ACTION_NEXT;
            } else if (imeAdapter.mTextInputType == ImeAdapter.sTextInputTypeNumber) {
                // Number
                outAttrs.inputType = InputType.TYPE_CLASS_NUMBER
                        | InputType.TYPE_NUMBER_VARIATION_NORMAL;
                outAttrs.imeOptions |= EditorInfo.IME_ACTION_NEXT;
            }
        }
    }

    private native boolean nativeSendSyntheticKeyEvent(int nativeImeAdapterAndroid,
            int eventType, long timestampMs, int keyCode, int unicodeChar);

    private native boolean nativeSendKeyEvent(int nativeImeAdapterAndroid, KeyEvent event,
            int action, int modifiers, long timestampMs, int keyCode, boolean isSystemKey,
            int unicodeChar);

    private native void nativeSetComposingText(int nativeImeAdapterAndroid, String text,
            int newCursorPosition);

    private native void nativeCommitText(int nativeImeAdapterAndroid, String text);

    private native void nativeAttachImeAdapter(int nativeImeAdapterAndroid);

    private native void nativeSetEditableSelectionOffsets(int nativeImeAdapterAndroid,
            int start, int end);

    private native void nativeSetComposingRegion(int nativeImeAdapterAndroid, int start, int end);

    private native void nativeDeleteSurroundingText(int nativeImeAdapterAndroid,
            int before, int after);

    private native void nativeUnselect(int nativeImeAdapterAndroid);
    private native void nativeSelectAll(int nativeImeAdapterAndroid);
    private native void nativeCut(int nativeImeAdapterAndroid);
    private native void nativeCopy(int nativeImeAdapterAndroid);
    private native void nativePaste(int nativeImeAdapterAndroid);
}
