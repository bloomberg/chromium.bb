// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.autofill.AutofillKeyboardAccessory;
import org.chromium.ui.autofill.AutofillKeyboardAccessory.AutofillKeyboardAccessoryDelegate;
import org.chromium.ui.autofill.AutofillSuggestion;
import org.chromium.ui.base.WindowAndroid;

/**
* JNI call glue for AutofillExternalDelagate C++ and Java objects.
* This provides an alternative UI for Autofill suggestions, and replaces AutofillPopupBridge when
* --enable-autofill-keyboard-accessory-view is passed on the command line.
*/
@JNINamespace("autofill")
public class AutofillKeyboardAccessoryBridge implements AutofillKeyboardAccessoryDelegate {
    private long mNativeAutofillKeyboardAccessory;
    private AutofillKeyboardAccessory mAccessoryView;

    private AutofillKeyboardAccessoryBridge() {
    }

    @CalledByNative
    private static AutofillKeyboardAccessoryBridge create() {
        return new AutofillKeyboardAccessoryBridge();
    }

    @Override
    public void dismissed() {
        if (mNativeAutofillKeyboardAccessory == 0) return;
        nativeViewDismissed(mNativeAutofillKeyboardAccessory);
    }

    @Override
    public void suggestionSelected(int listIndex) {
        if (mNativeAutofillKeyboardAccessory == 0) return;
        nativeSuggestionSelected(mNativeAutofillKeyboardAccessory, listIndex);
    }

    /**
     * Initializes this object.
     * This function should be called at most one time.
     * @param nativeAutofillKeyboardAccessory Handle to the native counterpart.
     * @param windowAndroid The window on which to show the suggestions.
     */
    @CalledByNative
    private void init(long nativeAutofillKeyboardAccessory, WindowAndroid windowAndroid) {
        if (windowAndroid == null || windowAndroid.getActivity().get() == null) {
            nativeViewDismissed(nativeAutofillKeyboardAccessory);
            dismissed();
            return;
        }

        mNativeAutofillKeyboardAccessory = nativeAutofillKeyboardAccessory;
        mAccessoryView = new AutofillKeyboardAccessory(windowAndroid, this);
    }

    /**
     * Clears the reference to the native view.
     */
    @CalledByNative
    private void resetNativeViewPointer() {
        mNativeAutofillKeyboardAccessory = 0;
    }

    /**
     * Hides the Autofill view.
     */
    @CalledByNative
    private void dismiss() {
        if (mAccessoryView != null) mAccessoryView.dismiss();
    }

    /**
     * Shows an Autofill view with specified suggestions.
     * @param suggestions Autofill suggestions to be displayed.
     */
    @CalledByNative
    private void show(AutofillSuggestion[] suggestions, boolean isRtl) {
        if (mAccessoryView != null) mAccessoryView.showWithSuggestions(suggestions, isRtl);
    }

    // Helper methods for AutofillSuggestion. These are copied from AutofillPopupBridge (which
    // should
    // eventually disappear).

    @CalledByNative
    private static AutofillSuggestion[] createAutofillSuggestionArray(int size) {
        return new AutofillSuggestion[size];
    }

    /**
     * @param array AutofillSuggestion array that should get a new suggestion added.
     * @param index Index in the array where to place a new suggestion.
     * @param label Suggested text. The text that's going to be filled in the focused field, with a
     *              few exceptions:
     *              <ul>
     *                  <li>Credit card numbers are elided, e.g. "Visa ****-1234."</li>
     *                  <li>The text "CLEAR FORM" will clear the filled in text.</li>
     *                  <li>Empty text can be used to display only icons, e.g. for credit card scan
     *                      or editing autofill settings.</li>
     *              </ul>
     * @param sublabel Hint for the suggested text. The text that's going to be filled in the
     *                 unfocused fields of the form. If {@see label} is empty, then this must be
     *                 empty too.
     * @param iconId The resource ID for the icon associated with the suggestion, or 0 for no icon.
     * @param suggestionId Identifier for the suggestion type.
     */
    @CalledByNative
    private static void addToAutofillSuggestionArray(AutofillSuggestion[] array, int index,
            String label, String sublabel, int iconId, int suggestionId) {
        int drawableId = iconId == 0 ? DropdownItem.NO_ICON : ResourceId.mapToDrawableId(iconId);
        array[index] = new AutofillSuggestion(label, sublabel, drawableId, suggestionId, false);
    }

    private native void nativeViewDismissed(long nativeAutofillKeyboardAccessoryView);
    private native void nativeSuggestionSelected(
            long nativeAutofillKeyboardAccessoryView, int listIndex);
}
