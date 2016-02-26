// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.text.Editable;
import android.text.InputType;
import android.text.Selection;
import android.util.StringBuilderPrinter;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.CorrectionInfo;
import android.view.inputmethod.EditorInfo;

import org.chromium.base.ThreadUtils;
import org.chromium.blink_public.web.WebTextInputFlags;
import org.chromium.ui.base.ime.TextInputType;

import java.util.Locale;

/**
 * Utilities for IME such as computing outAttrs, and dumping object information.
 */
public class ImeUtils {
    /**
     * Compute {@link EditorInfo} based on the given parameters. This is needed for
     * {@link View#onCreateInputConnection(EditorInfo)}.
     *
     * @param inputType Type defined in {@link TextInputType}.
     * @param inputFlags Flags defined in {@link WebTextInputFlags}.
     * @param initialSelStart The initial selection start position.
     * @param initialSelEnd The initial selection end position.
     * @param outAttrs An instance of {@link EditorInfo} that we are going to change.
     */
    public static void computeEditorInfo(int inputType, int inputFlags, int initialSelStart,
            int initialSelEnd, EditorInfo outAttrs) {
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_FLAG_NO_EXTRACT_UI;
        outAttrs.inputType =
                EditorInfo.TYPE_CLASS_TEXT | EditorInfo.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT;

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
        } else if (inputType == TextInputType.PASSWORD) {
            // Password
            outAttrs.inputType =
                    InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD;
            outAttrs.imeOptions |= EditorInfo.IME_ACTION_GO;
        } else if (inputType == TextInputType.SEARCH) {
            // Search
            outAttrs.imeOptions |= EditorInfo.IME_ACTION_SEARCH;
        } else if (inputType == TextInputType.URL) {
            // Url
            outAttrs.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI;
            outAttrs.imeOptions |= EditorInfo.IME_ACTION_GO;
        } else if (inputType == TextInputType.EMAIL) {
            // Email
            outAttrs.inputType =
                    InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS;
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
                    | InputType.TYPE_NUMBER_VARIATION_NORMAL | InputType.TYPE_NUMBER_FLAG_DECIMAL;
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
    }

    public static String getEditorInfoDebugString(EditorInfo editorInfo) {
        StringBuilder builder = new StringBuilder();
        StringBuilderPrinter printer = new StringBuilderPrinter(builder);
        editorInfo.dump(printer, "");
        return builder.toString();
    }

    public static String getEditableDebugString(Editable editable) {
        return String.format(Locale.US, "Editable {[%s] SEL[%d %d] COM[%d %d]}",
                editable.toString(), Selection.getSelectionStart(editable),
                Selection.getSelectionEnd(editable),
                BaseInputConnection.getComposingSpanStart(editable),
                BaseInputConnection.getComposingSpanEnd(editable));
    }

    /**
     * Dump the given {@CorrectionInfo} into class.
     * @param correctionInfo
     * @return User-readable {@CorrectionInfo}.
     */
    static String getCorrectInfoDebugString(CorrectionInfo correctionInfo) {
        // TODO(changwan): implement it properly if needed.
        return correctionInfo.toString();
    }

    // TODO(changwan): remove these once implementation becomes stable.
    static void checkCondition(boolean value) {
        if (!value) {
            throw new AssertionError();
        }
    }

    static void checkCondition(String msg, boolean value) {
        if (!value) {
            throw new AssertionError(msg);
        }
    }

    static void checkOnUiThread() {
        checkCondition("Should be on UI thread.", ThreadUtils.runningOnUiThread());
    }
}