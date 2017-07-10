// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.graphics.Rect;
import android.os.StrictMode;
import android.support.annotation.CallSuper;
import android.text.Editable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.EditText;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.widget.VerticallyFixedEditText;

/**
 * An {@link EditText} that shows autocomplete text at the end.
 */
public class AutocompleteEditText
        extends VerticallyFixedEditText implements AutocompleteEditTextModelBase.Delegate {
    private static final String TAG = "cr_AutocompleteEdit";

    private static final boolean DEBUG = false;

    private final AccessibilityManager mAccessibilityManager;

    private AutocompleteEditTextModelBase mModel;
    private boolean mIgnoreTextChangesForAutocomplete = true;
    private boolean mLastEditWasPaste;

    /**
     * Whether default TextView scrolling should be disabled because autocomplete has been added.
     * This allows the user entered text to be shown instead of the end of the autocomplete.
     */
    private boolean mDisableTextScrollingFromAutocomplete;

    private boolean mIgnoreImeForTest;

    public AutocompleteEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAccessibilityManager =
                (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
    }

    private void ensureModel() {
        if (mModel != null) return;

        if (ChromeFeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)) {
            Log.i(TAG, "Using spannable model...");
            mModel = new SpannableAutocompleteEditTextModel(this);
        } else {
            Log.i(TAG, "Using non-spannable model...");
            mModel = new AutocompleteEditTextModel(this);
        }
        // Feed initial values.
        mModel.setIgnoreTextChangeFromAutocomplete(true);
        mModel.onFocusChanged(hasFocus());
        mModel.onSetText(getText());
        mModel.onTextChanged(getText(), 0, 0, getText().length());
        mModel.onSelectionChanged(getSelectionStart(), getSelectionEnd());
        if (mLastEditWasPaste) mModel.onPaste();
        mModel.setIgnoreTextChangeFromAutocomplete(false);
        mModel.setIgnoreTextChangeFromAutocomplete(mIgnoreTextChangesForAutocomplete);
    }

    /**
     * Sets whether text changes should trigger autocomplete.
     *
     * @param ignoreAutocomplete Whether text changes should be ignored and no auto complete
     *                           triggered.
     */
    public void setIgnoreTextChangesForAutocomplete(boolean ignoreAutocomplete) {
        mIgnoreTextChangesForAutocomplete = ignoreAutocomplete;
        if (mModel != null) mModel.setIgnoreTextChangeFromAutocomplete(ignoreAutocomplete);
    }

    /**
     * @return The user text without the autocomplete text.
     */
    public String getTextWithoutAutocomplete() {
        if (mModel == null) return "";
        return mModel.getTextWithoutAutocomplete();
    }

    /** @return Text that includes autocomplete. */
    public String getTextWithAutocomplete() {
        if (mModel == null) return "";
        return mModel.getTextWithAutocomplete();
    }

    /** @return Whether any autocomplete information is specified on the current text. */
    @VisibleForTesting
    public boolean hasAutocomplete() {
        if (mModel == null) return false;
        return mModel.hasAutocomplete();
    }

    /**
     * Whether we want to be showing inline autocomplete results. We don't want to show them as the
     * user deletes input. Also if there is a composition (e.g. while using the Japanese IME),
     * we must not autocomplete or we'll destroy the composition.
     * @return Whether we want to be showing inline autocomplete results.
     */
    public boolean shouldAutocomplete() {
        if (mModel == null) return false;
        return mModel.shouldAutocomplete();
    }

    @Override
    protected void onSelectionChanged(int selStart, int selEnd) {
        if (mModel != null) mModel.onSelectionChanged(selStart, selEnd);
        super.onSelectionChanged(selStart, selEnd);
    }

    @Override
    protected void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
        if (mModel != null) mModel.onFocusChanged(focused);
        super.onFocusChanged(focused, direction, previouslyFocusedRect);
    }

    @Override
    public boolean bringPointIntoView(int offset) {
        if (mDisableTextScrollingFromAutocomplete) return false;
        return super.bringPointIntoView(offset);
    }

    @Override
    public boolean onPreDraw() {
        boolean retVal = super.onPreDraw();
        if (mDisableTextScrollingFromAutocomplete) {
            // super.onPreDraw will put the selection at the end of the text selection, but
            // in the case of autocomplete we want the last typed character to be shown, which
            // is the start of selection.
            mDisableTextScrollingFromAutocomplete = false;
            bringPointIntoView(getSelectionStart());
            retVal = true;
        }
        return retVal;
    }

    /** Call this when text is pasted. */
    @CallSuper
    public void onPaste() {
        mLastEditWasPaste = true;
        if (mModel != null) mModel.onPaste();
    }

    /**
     * Autocompletes the text and selects the text that was not entered by the user. Using append()
     * instead of setText() to preserve the soft-keyboard layout.
     * @param userText user The text entered by the user.
     * @param inlineAutocompleteText The suggested autocompletion for the user's text.
     */
    public void setAutocompleteText(CharSequence userText, CharSequence inlineAutocompleteText) {
        boolean emptyAutocomplete = TextUtils.isEmpty(inlineAutocompleteText);
        if (!emptyAutocomplete) mDisableTextScrollingFromAutocomplete = true;
        if (mModel != null) mModel.setAutocompleteText(userText, inlineAutocompleteText);
    }

    /**
     * Returns the length of the autocomplete text currently displayed, zero if none is
     * currently displayed.
     */
    public int getAutocompleteLength() {
        if (mModel == null) return 0;
        return mModel.getAutocompleteText().length();
    }

    @Override
    protected void onTextChanged(CharSequence text, int start, int lengthBefore, int lengthAfter) {
        super.onTextChanged(text, start, lengthBefore, lengthAfter);
        mLastEditWasPaste = false;
        if (mModel != null) mModel.onTextChanged(text, start, lengthBefore, lengthAfter);
    }

    @Override
    public void setText(CharSequence text, BufferType type) {
        if (DEBUG) Log.i(TAG, "setText -- text: %s", text);
        mDisableTextScrollingFromAutocomplete = false;

        // Avoid setting the same text as it will mess up the scroll/cursor position.
        // Setting the text is also quite expensive, so only do it when the text has changed
        // (since we apply spans when the view is not focused, we only optimize this when the
        // text is being edited).
        if (!TextUtils.equals(getEditableText(), text)) {
            // Certain OEM implementations of setText trigger disk reads. crbug.com/633298
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            try {
                super.setText(text, type);
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }
        }
        if (mModel != null) mModel.onSetText(text);
    }

    @Override
    public void sendAccessibilityEventUnchecked(AccessibilityEvent event) {
        if (mIgnoreTextChangesForAutocomplete) {
            if (event.getEventType() == AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED
                    || event.getEventType() == AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED) {
                if (DEBUG) Log.i(TAG, "Ignoring accessibility event from autocomplete.");
                return;
            }
        }
        super.sendAccessibilityEventUnchecked(event);
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        // Certain OEM implementations of onInitializeAccessibilityNodeInfo trigger disk reads
        // to access the clipboard.  crbug.com/640993
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            super.onInitializeAccessibilityNodeInfo(info);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @VisibleForTesting
    public InputConnection getInputConnection() {
        if (mModel == null) return null;
        return mModel.getInputConnection();
    }

    @VisibleForTesting
    public void setIgnoreImeForTest(boolean ignore) {
        mIgnoreImeForTest = ignore;
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        InputConnection target = super.onCreateInputConnection(outAttrs);
        // Initially, target is null until View gets the focus.
        if (target == null && mModel == null) {
            if (DEBUG) Log.i(TAG, "onCreateInputConnection - ignoring null target.");
            return null;
        }
        if (DEBUG) Log.i(TAG, "onCreateInputConnection: " + target);
        ensureModel();
        InputConnection retVal = mModel.onCreateInputConnection(target);
        if (mIgnoreImeForTest) return null;
        return retVal;
    }

    @Override
    public boolean dispatchKeyEvent(final KeyEvent event) {
        if (mIgnoreImeForTest) return true;
        if (mModel == null) return super.dispatchKeyEvent(event);
        return mModel.dispatchKeyEvent(event);
    }

    @Override
    public boolean super_dispatchKeyEvent(KeyEvent event) {
        return super.dispatchKeyEvent(event);
    }

    /**
     * @return Whether the current UrlBar input has been pasted from the clipboard.
     */
    public boolean wasLastEditPaste() {
        return mLastEditWasPaste;
    }

    @Override
    public void replaceAllTextFromAutocomplete(String text) {
        assert false; // make sure that this method is properly overridden.
    }

    @Override
    public void onAutocompleteTextStateChanged(boolean updateDisplay) {
        assert false; // make sure that this method is properly overridden.
    }

    @Override
    public void onNoChangeTypingAccessibilityEvent(int selectionStart) {
        if (DEBUG) Log.i(TAG, "onNoChangeTypingAccessibilityEvent: " + selectionStart);
        // Since the text isn't changing, TalkBack won't read out the typed characters.
        // To work around this, explicitly send an accessibility event. crbug.com/416595
        Editable currentText = getText();
        if (mAccessibilityManager != null && mAccessibilityManager.isEnabled()) {
            AccessibilityEvent event =
                    AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED);
            event.setFromIndex(selectionStart);
            event.setRemovedCount(0);
            event.setAddedCount(1);
            event.setBeforeText(currentText.toString().substring(0, selectionStart));
            sendAccessibilityEventUnchecked(event);
        }
    }

    @Override
    public void onUpdateSelectionForTesting(int selStart, int selEnd) {}
}
