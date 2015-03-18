// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.res.Configuration;
import android.os.Handler;
import android.os.ResultReceiver;
import android.os.SystemClock;
import android.text.Editable;
import android.text.SpannableString;
import android.text.style.BackgroundColorSpan;
import android.text.style.CharacterStyle;
import android.text.style.UnderlineSpan;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.VisibleForTesting;
import org.chromium.blink_public.web.WebInputEventModifier;
import org.chromium.blink_public.web.WebInputEventType;
import org.chromium.blink_public.web.WebTextInputFlags;
import org.chromium.ui.base.ime.TextInputType;
import org.chromium.ui.picker.InputDialogContainer;

import java.lang.CharSequence;

/**
 * Adapts and plumbs android IME service onto the chrome text input API.
 * ImeAdapter provides an interface in both ways native <-> java:
 * 1. InputConnectionAdapter notifies native code of text composition state and
 *    dispatch key events from java -> WebKit.
 * 2. Native ImeAdapter notifies java side to clear composition text.
 *
 * The basic flow is:
 * 1. When InputConnectionAdapter gets called with composition or result text:
 *    If we receive a composition text or a result text, then we just need to
 *    dispatch a synthetic key event with special keycode 229, and then dispatch
 *    the composition or result text.
 * 2. Intercept dispatchKeyEvent() method for key events not handled by IME, we
 *   need to dispatch them to webkit and check webkit's reply. Then inject a
 *   new key event for further processing if webkit didn't handle it.
 *
 * Note that the native peer object does not take any strong reference onto the
 * instance of this java object, hence it is up to the client of this class (e.g.
 * the ViewEmbedder implementor) to hold a strong reference to it for the required
 * lifetime of the object.
 */
@JNINamespace("content")
public class ImeAdapter {

    /**
     * Interface for the delegate that needs to be notified of IME changes.
     */
    public interface ImeAdapterDelegate {
        /**
         * Called to notify the delegate about synthetic/real key events before sending to renderer.
         */
        void onImeEvent();

        /**
         * Called when a request to hide the keyboard is sent to InputMethodManager.
         */
        void onDismissInput();

        /**
         * Called when the keyboard could not be shown due to the hardware keyboard being present.
         */
        void onKeyboardBoundsUnchanged();

        /**
         * @return View that the keyboard should be attached to.
         */
        View getAttachedView();

        /**
         * @return Object that should be called for all keyboard show and hide requests.
         */
        ResultReceiver getNewShowKeyboardReceiver();
    }

    private static final int COMPOSITION_KEY_CODE = 229;

    // Delay introduced to avoid hiding the keyboard if new show requests are received.
    // The time required by the unfocus-focus events triggered by tab has been measured in soju:
    // Mean: 18.633 ms, Standard deviation: 7.9837 ms.
    // The value here should be higher enough to cover these cases, but not too high to avoid
    // letting the user perceiving important delays.
    private static final int INPUT_DISMISS_DELAY = 150;
    private final Runnable mDismissInputRunnable = new Runnable() {
        @Override
        public void run() {
            dismissInput(true);
        }
    };

    static char[] sSingleCharArray = new char[1];
    static KeyCharacterMap sKeyCharacterMap;

    private long mNativeImeAdapterAndroid;
    private InputMethodManagerWrapper mInputMethodManagerWrapper;
    private AdapterInputConnection mInputConnection;
    private final ImeAdapterDelegate mViewEmbedder;
    private final Handler mHandler;
    private int mTextInputType;
    private int mTextInputFlags;
    private String mLastComposeText;

    @VisibleForTesting
    int mLastSyntheticKeyCode;

    @VisibleForTesting
    boolean mIsShowWithoutHideOutstanding = false;

    /**
     * @param wrapper InputMethodManagerWrapper that should receive all the call directed to
     *                InputMethodManager.
     * @param embedder The view that is used for callbacks from ImeAdapter.
     */
    public ImeAdapter(InputMethodManagerWrapper wrapper, ImeAdapterDelegate embedder) {
        mInputMethodManagerWrapper = wrapper;
        mViewEmbedder = embedder;
        mHandler = new Handler();
    }

    /**
     * Default factory for AdapterInputConnection classes.
     */
    public static class AdapterInputConnectionFactory {
        public AdapterInputConnection get(View view, ImeAdapter imeAdapter,
                Editable editable, EditorInfo outAttrs) {
            return new AdapterInputConnection(view, imeAdapter, editable, outAttrs);
        }
    }

    /**
     * Overrides the InputMethodManagerWrapper that ImeAdapter uses to make calls to
     * InputMethodManager.
     * @param immw InputMethodManagerWrapper that should be used to call InputMethodManager.
     */
    @VisibleForTesting
    public void setInputMethodManagerWrapper(InputMethodManagerWrapper immw) {
        mInputMethodManagerWrapper = immw;
    }

    /**
     * Should be only used by AdapterInputConnection.
     * @return InputMethodManagerWrapper that should receive all the calls directed to
     *         InputMethodManager.
     */
    InputMethodManagerWrapper getInputMethodManagerWrapper() {
        return mInputMethodManagerWrapper;
    }

    /**
     * Set the current active InputConnection when a new InputConnection is constructed.
     * @param inputConnection The input connection that is currently used with IME.
     */
    void setInputConnection(AdapterInputConnection inputConnection) {
        mInputConnection = inputConnection;
        mLastComposeText = null;
    }

    /**
     * Should be used only by AdapterInputConnection.
     * @return The input type of currently focused element.
     */
    int getTextInputType() {
        return mTextInputType;
    }

    /**
     * Should be used only by AdapterInputConnection.
     * @return The input flags of the currently focused element.
     */
    int getTextInputFlags() {
        return mTextInputFlags;
    }

    private static int getModifiers(int metaState) {
        int modifiers = 0;
        if ((metaState & KeyEvent.META_SHIFT_ON) != 0) {
            modifiers |= WebInputEventModifier.ShiftKey;
        }
        if ((metaState & KeyEvent.META_ALT_ON) != 0) {
            modifiers |= WebInputEventModifier.AltKey;
        }
        if ((metaState & KeyEvent.META_CTRL_ON) != 0) {
            modifiers |= WebInputEventModifier.ControlKey;
        }
        if ((metaState & KeyEvent.META_CAPS_LOCK_ON) != 0) {
            modifiers |= WebInputEventModifier.CapsLockOn;
        }
        if ((metaState & KeyEvent.META_NUM_LOCK_ON) != 0) {
            modifiers |= WebInputEventModifier.NumLockOn;
        }
        return modifiers;
    }

    /**
     * Shows or hides the keyboard based on passed parameters.
     * @param nativeImeAdapter Pointer to the ImeAdapterAndroid object that is sending the update.
     * @param textInputType Text input type for the currently focused field in renderer.
     * @param showIfNeeded Whether the keyboard should be shown if it is currently hidden.
     */
    public void updateKeyboardVisibility(long nativeImeAdapter, int textInputType,
            int textInputFlags, boolean showIfNeeded) {
        // If current input type is none and showIfNeeded is false, IME should not be shown
        // and input type should remain as none.
        if (mTextInputType == TextInputType.NONE && !showIfNeeded) {
            return;
        }

        if (mNativeImeAdapterAndroid != nativeImeAdapter || mTextInputType != textInputType) {
            // We have to attach immediately, even if we're going to delay the dismissing of
            // currently visible keyboard because otherwise we have a race condition: If the
            // native IME adapter gets destructed before the delayed-dismiss fires, we'll access
            // an object that has been already released.  http://crbug.com/447287
            attach(nativeImeAdapter, textInputType, textInputFlags, true);

            if (mTextInputType != TextInputType.NONE) {
                mInputMethodManagerWrapper.restartInput(mViewEmbedder.getAttachedView());
                if (showIfNeeded) {
                    showKeyboard();
                }
            }
        } else if (hasInputType() && showIfNeeded) {
            showKeyboard();
        }
    }

    private void attach(long nativeImeAdapter, int textInputType, int textInputFlags,
            boolean delayDismissInput) {
        if (mNativeImeAdapterAndroid != 0) {
            nativeResetImeAdapter(mNativeImeAdapterAndroid);
        }
        if (nativeImeAdapter != 0) {
            nativeAttachImeAdapter(nativeImeAdapter);
        }
        mNativeImeAdapterAndroid = nativeImeAdapter;
        mLastComposeText = null;
        mTextInputFlags = textInputFlags;
        if (textInputType == mTextInputType) return;
        mTextInputType = textInputType;
        mHandler.removeCallbacks(mDismissInputRunnable);  // okay if not found
        if (mTextInputType == TextInputType.NONE) {
            if (delayDismissInput) {
                // Set a delayed task to do unfocus. This avoids hiding the keyboard when tabbing
                // through text inputs or when JS rapidly changes focus to another text element.
                mHandler.postDelayed(mDismissInputRunnable, INPUT_DISMISS_DELAY);
                mIsShowWithoutHideOutstanding = false;
            } else {
                // Some things (including tests) expect the keyboard to be dismissed immediately.
                dismissInput(true);
            }
        }
    }

    /**
     * Attaches the imeAdapter to its native counterpart. This is needed to start forwarding
     * keyboard events to WebKit.
     * @param nativeImeAdapter The pointer to the native ImeAdapter object.
     */
    public void attach(long nativeImeAdapter) {
        attach(nativeImeAdapter, TextInputType.NONE, WebTextInputFlags.None, false);
    }

    private void showKeyboard() {
        mIsShowWithoutHideOutstanding = true;
        mInputMethodManagerWrapper.showSoftInput(
                mViewEmbedder.getAttachedView(), 0, mViewEmbedder.getNewShowKeyboardReceiver());
        if (mViewEmbedder.getAttachedView().getResources().getConfiguration().keyboard
                != Configuration.KEYBOARD_NOKEYS) {
            mViewEmbedder.onKeyboardBoundsUnchanged();
        }
    }

    private void dismissInput(boolean unzoomIfNeeded) {
        mIsShowWithoutHideOutstanding = false;
        View view = mViewEmbedder.getAttachedView();
        if (mInputMethodManagerWrapper.isActive(view)) {
            mInputMethodManagerWrapper.hideSoftInputFromWindow(view.getWindowToken(), 0,
                    unzoomIfNeeded ? mViewEmbedder.getNewShowKeyboardReceiver() : null);
        }
        mViewEmbedder.onDismissInput();
    }

    private boolean hasInputType() {
        return mTextInputType != TextInputType.NONE;
    }

    private static boolean isTextInputType(int type) {
        return type != TextInputType.NONE && !InputDialogContainer.isDialogInputType(type);
    }

    public boolean hasTextInputType() {
        return isTextInputType(mTextInputType);
    }

    /**
     * @return true if the selected text is of password.
     */
    public boolean isSelectionPassword() {
        return mTextInputType == TextInputType.PASSWORD;
    }

    public boolean dispatchKeyEvent(KeyEvent event) {
        // Physical keyboards have their events come through here instead of
        // AdapterInputConnection.
        if (mInputConnection != null) {
            return mInputConnection.sendKeyEvent(event);
        }
        return translateAndSendNativeEvents(event, 0);
    }

    private int shouldSendKeyEventWithKeyCode(String text) {
        if (text.length() != 1) return COMPOSITION_KEY_CODE;

        if (text.equals("\n")) return KeyEvent.KEYCODE_ENTER;
        else if (text.equals("\t")) return KeyEvent.KEYCODE_TAB;
        else return COMPOSITION_KEY_CODE;
    }

    /**
     * @return Android KeyEvent for a single unicode character.  Only one KeyEvent is returned
     * even if the system determined that various modifier keys (like Shift) would also have
     * been pressed.
     */
    private static KeyEvent androidKeyEventForCharacter(char chr) {
        if (sKeyCharacterMap == null) {
            sKeyCharacterMap = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);
        }
        sSingleCharArray[0] = chr;
        // TODO: Evaluate cost of this system call.
        KeyEvent[] events = sKeyCharacterMap.getEvents(sSingleCharArray);
        if (events == null) {  // No known key sequence will create that character.
            return null;
        }

        for (int i = 0; i < events.length; ++i) {
            if (events[i].getAction() == KeyEvent.ACTION_DOWN
                    && !KeyEvent.isModifierKey(events[i].getKeyCode())) {
                return events[i];
            }
        }

        return null;  // No printing characters were found.
    }

    @VisibleForTesting
    public static KeyEvent getTypedKeyEventGuess(String oldtext, String newtext) {
        // Starting typing a new composition should add only a single character.  Any composition
        // beginning with text longer than that must come from something other than typing so
        // return 0.
        if (oldtext == null) {
            if (newtext.length() == 1) {
                return androidKeyEventForCharacter(newtext.charAt(0));
            } else {
                return null;
            }
        }

        // The content has grown in length: assume the last character is the key that caused it.
        if (newtext.length() > oldtext.length() && newtext.startsWith(oldtext))
            return androidKeyEventForCharacter(newtext.charAt(newtext.length() - 1));

        // The content has shrunk in length: assume that backspace was pressed.
        if (oldtext.length() > newtext.length() && oldtext.startsWith(newtext))
            return new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL);

        // The content is unchanged or has undergone a complex change (i.e. not a simple tail
        // modification) so return an unknown key-code.
        return null;
    }

    void sendKeyEventWithKeyCode(int keyCode, int flags) {
        long eventTime = SystemClock.uptimeMillis();
        mLastSyntheticKeyCode = keyCode;
        translateAndSendNativeEvents(new KeyEvent(eventTime, eventTime,
                KeyEvent.ACTION_DOWN, keyCode, 0, 0,
                KeyCharacterMap.VIRTUAL_KEYBOARD, 0,
                flags), 0);
        translateAndSendNativeEvents(new KeyEvent(SystemClock.uptimeMillis(), eventTime,
                KeyEvent.ACTION_UP, keyCode, 0, 0,
                KeyCharacterMap.VIRTUAL_KEYBOARD, 0,
                flags), 0);
    }

    // Calls from Java to C++
    // TODO: Add performance tracing to more complicated functions.

    boolean checkCompositionQueueAndCallNative(CharSequence text, int newCursorPosition,
            boolean isCommit) {
        if (mNativeImeAdapterAndroid == 0) return false;
        mViewEmbedder.onImeEvent();

        String textStr = text.toString();
        int keyCode = shouldSendKeyEventWithKeyCode(textStr);
        long timeStampMs = SystemClock.uptimeMillis();

        if (keyCode != COMPOSITION_KEY_CODE) {
            sendKeyEventWithKeyCode(keyCode,
                    KeyEvent.FLAG_SOFT_KEYBOARD | KeyEvent.FLAG_KEEP_TOUCH_MODE);
        } else {
            KeyEvent keyEvent = getTypedKeyEventGuess(mLastComposeText, textStr);
            int modifiers = 0;
            if (keyEvent != null) {
                keyCode = keyEvent.getKeyCode();
                modifiers = getModifiers(keyEvent.getMetaState());
            } else if (!textStr.equals(mLastComposeText)) {
                keyCode = KeyEvent.KEYCODE_UNKNOWN;
            } else {
                keyCode = -1;
            }

            // If this is a single-character commit with no previous composition, then treat it as
            // a native KeyDown/KeyUp pair with no composition rather than a synthetic pair with
            // composition below.
            if (keyCode > 0 && isCommit && mLastComposeText == null && textStr.length() == 1) {
                mLastSyntheticKeyCode = keyCode;
                return translateAndSendNativeEvents(keyEvent, 0)
                        && translateAndSendNativeEvents(
                                KeyEvent.changeAction(keyEvent, KeyEvent.ACTION_UP), 0);
            }

            // If we do not have autocomplete=off, then always send compose events rather than a
            // guessed keyCode. This addresses http://crbug.com/422685 .
            if ((mTextInputFlags & WebTextInputFlags.AutocompleteOff) == 0) {
                keyCode = COMPOSITION_KEY_CODE;
                modifiers = 0;
            }

            // When typing, there is no issue sending KeyDown and KeyUp events around the
            // composition event because those key events do nothing (other than call JS
            // handlers).  Typing does not cause changes outside of a KeyPress event which
            // we don't call here.  However, if the key-code is a control key such as
            // KEYCODE_DEL then there never is an associated KeyPress event and the KeyDown
            // event itself causes the action.  The net result below is that the Renderer calls
            // cancelComposition() and then Android starts anew with setComposingRegion().
            // This stopping and restarting of composition could be a source of problems
            // with 3rd party keyboards.
            //
            // An alternative is to *not* call nativeSetComposingText() in the non-commit case
            // below.  This avoids the restart of composition described above but fails to send
            // an update to the composition while in composition which, strictly speaking,
            // does not match the spec.
            //
            // For now, the solution is to endure the restarting of composition and only dive
            // into the alternate solution should there be problems in the field.  --bcwhite

            if (keyCode >= 0) {
                nativeSendSyntheticKeyEvent(mNativeImeAdapterAndroid, WebInputEventType.RawKeyDown,
                        timeStampMs, keyCode, modifiers, 0);
            }

            if (isCommit) {
                nativeCommitText(mNativeImeAdapterAndroid, textStr);
                textStr = null;
            } else {
                nativeSetComposingText(mNativeImeAdapterAndroid, text, textStr, newCursorPosition);
            }

            if (keyCode >= 0) {
                nativeSendSyntheticKeyEvent(mNativeImeAdapterAndroid, WebInputEventType.KeyUp,
                        timeStampMs, keyCode, modifiers, 0);
            }

            mLastSyntheticKeyCode = keyCode;
        }

        mLastComposeText = textStr;
        return true;
    }

    void finishComposingText() {
        mLastComposeText = null;
        if (mNativeImeAdapterAndroid == 0) return;
        nativeFinishComposingText(mNativeImeAdapterAndroid);
    }

    boolean translateAndSendNativeEvents(KeyEvent event, int accentChar) {
        if (mNativeImeAdapterAndroid == 0) return false;

        int action = event.getAction();
        if (action != KeyEvent.ACTION_DOWN && action != KeyEvent.ACTION_UP) {
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
        mViewEmbedder.onImeEvent();
        int unicodeChar = AdapterInputConnection.maybeAddAccentToCharacter(
                accentChar, event.getUnicodeChar());
        return nativeSendKeyEvent(mNativeImeAdapterAndroid, event, event.getAction(),
                getModifiers(event.getMetaState()), event.getEventTime(), event.getKeyCode(),
                             /*isSystemKey=*/false, unicodeChar);
    }

    boolean sendSyntheticKeyEvent(int eventType, long timestampMs, int keyCode, int modifiers,
            int unicodeChar) {
        if (mNativeImeAdapterAndroid == 0) return false;

        nativeSendSyntheticKeyEvent(
                mNativeImeAdapterAndroid, eventType, timestampMs, keyCode, modifiers, unicodeChar);
        return true;
    }

    /**
     * Send a request to the native counterpart to delete a given range of characters.
     * @param beforeLength Number of characters to extend the selection by before the existing
     *                     selection.
     * @param afterLength Number of characters to extend the selection by after the existing
     *                    selection.
     * @return Whether the native counterpart of ImeAdapter received the call.
     */
    boolean deleteSurroundingText(int beforeLength, int afterLength) {
        mViewEmbedder.onImeEvent();
        if (mNativeImeAdapterAndroid == 0) return false;
        nativeDeleteSurroundingText(mNativeImeAdapterAndroid, beforeLength, afterLength);
        return true;
    }

    /**
     * Send a request to the native counterpart to set the selection to given range.
     * @param start Selection start index.
     * @param end Selection end index.
     * @return Whether the native counterpart of ImeAdapter received the call.
     */
    boolean setEditableSelectionOffsets(int start, int end) {
        if (mNativeImeAdapterAndroid == 0) return false;
        nativeSetEditableSelectionOffsets(mNativeImeAdapterAndroid, start, end);
        return true;
    }

    /**
     * Send a request to the native counterpart to set composing region to given indices.
     * @param start The start of the composition.
     * @param end The end of the composition.
     * @return Whether the native counterpart of ImeAdapter received the call.
     */
    boolean setComposingRegion(CharSequence text, int start, int end) {
        if (mNativeImeAdapterAndroid == 0) return false;
        nativeSetComposingRegion(mNativeImeAdapterAndroid, start, end);
        mLastComposeText = text != null ? text.toString() : null;
        return true;
    }

    /**
     * Send a request to the native counterpart to unselect text.
     * @return Whether the native counterpart of ImeAdapter received the call.
     */
    public boolean unselect() {
        if (mNativeImeAdapterAndroid == 0) return false;
        nativeUnselect(mNativeImeAdapterAndroid);
        return true;
    }

    /**
     * Send a request to the native counterpart of ImeAdapter to select all the text.
     * @return Whether the native counterpart of ImeAdapter received the call.
     */
    public boolean selectAll() {
        if (mNativeImeAdapterAndroid == 0) return false;
        nativeSelectAll(mNativeImeAdapterAndroid);
        return true;
    }

    /**
     * Send a request to the native counterpart of ImeAdapter to cut the selected text.
     * @return Whether the native counterpart of ImeAdapter received the call.
     */
    public boolean cut() {
        if (mNativeImeAdapterAndroid == 0) return false;
        nativeCut(mNativeImeAdapterAndroid);
        return true;
    }

    /**
     * Send a request to the native counterpart of ImeAdapter to copy the selected text.
     * @return Whether the native counterpart of ImeAdapter received the call.
     */
    public boolean copy() {
        if (mNativeImeAdapterAndroid == 0) return false;
        nativeCopy(mNativeImeAdapterAndroid);
        return true;
    }

    /**
     * Send a request to the native counterpart of ImeAdapter to paste the text from the clipboard.
     * @return Whether the native counterpart of ImeAdapter received the call.
     */
    public boolean paste() {
        if (mNativeImeAdapterAndroid == 0) return false;
        nativePaste(mNativeImeAdapterAndroid);
        return true;
    }

    // Calls from C++ to Java

    @CalledByNative
    private void focusedNodeChanged(boolean isEditable) {
        if (mInputConnection != null && isEditable) mInputConnection.restartInput();
    }

    @CalledByNative
    private void populateUnderlinesFromSpans(CharSequence text, long underlines) {
        if (!(text instanceof SpannableString)) return;

        SpannableString spannableString = ((SpannableString) text);
        CharacterStyle spans[] =
                spannableString.getSpans(0, text.length(), CharacterStyle.class);
        for (CharacterStyle span : spans) {
            if (span instanceof BackgroundColorSpan) {
                nativeAppendBackgroundColorSpan(underlines, spannableString.getSpanStart(span),
                        spannableString.getSpanEnd(span),
                        ((BackgroundColorSpan) span).getBackgroundColor());
            } else if (span instanceof UnderlineSpan) {
                nativeAppendUnderlineSpan(underlines, spannableString.getSpanStart(span),
                        spannableString.getSpanEnd(span));
            }
        }
    }

    @CalledByNative
    private void cancelComposition() {
        if (mInputConnection != null) mInputConnection.restartInput();
        mLastComposeText = null;
    }

    @CalledByNative
    void detach() {
        mHandler.removeCallbacks(mDismissInputRunnable);
        mNativeImeAdapterAndroid = 0;
        mTextInputType = 0;
    }

    private native boolean nativeSendSyntheticKeyEvent(long nativeImeAdapterAndroid,
            int eventType, long timestampMs, int keyCode, int modifiers, int unicodeChar);

    private native boolean nativeSendKeyEvent(long nativeImeAdapterAndroid, KeyEvent event,
            int action, int modifiers, long timestampMs, int keyCode, boolean isSystemKey,
            int unicodeChar);

    private static native void nativeAppendUnderlineSpan(long underlinePtr, int start, int end);

    private static native void nativeAppendBackgroundColorSpan(long underlinePtr, int start,
            int end, int backgroundColor);

    private native void nativeSetComposingText(long nativeImeAdapterAndroid, CharSequence text,
            String textStr, int newCursorPosition);

    private native void nativeCommitText(long nativeImeAdapterAndroid, String textStr);

    private native void nativeFinishComposingText(long nativeImeAdapterAndroid);

    private native void nativeAttachImeAdapter(long nativeImeAdapterAndroid);

    private native void nativeSetEditableSelectionOffsets(long nativeImeAdapterAndroid,
            int start, int end);

    private native void nativeSetComposingRegion(long nativeImeAdapterAndroid, int start, int end);

    private native void nativeDeleteSurroundingText(long nativeImeAdapterAndroid,
            int before, int after);

    private native void nativeUnselect(long nativeImeAdapterAndroid);
    private native void nativeSelectAll(long nativeImeAdapterAndroid);
    private native void nativeCut(long nativeImeAdapterAndroid);
    private native void nativeCopy(long nativeImeAdapterAndroid);
    private native void nativePaste(long nativeImeAdapterAndroid);
    private native void nativeResetImeAdapter(long nativeImeAdapterAndroid);
}
