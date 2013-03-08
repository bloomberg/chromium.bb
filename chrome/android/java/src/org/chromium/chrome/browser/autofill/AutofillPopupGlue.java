// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.autofill.AutofillPopup.AutofillPopupDelegate;
import org.chromium.content.browser.ContainerViewDelegate;
import org.chromium.ui.gfx.NativeWindow;

/**
* JNI call glue for AutofillExternalDelagate C++ and Java objects.
*/
public class AutofillPopupGlue implements AutofillPopupDelegate{
    private final int mNativeAutofillPopup;
    private final AutofillPopup mAutofillPopup;

    public AutofillPopupGlue(int nativeAutofillPopupViewAndroid, NativeWindow nativeWindow,
            ContainerViewDelegate containerViewDelegate) {
        mNativeAutofillPopup = nativeAutofillPopupViewAndroid;
        mAutofillPopup = new AutofillPopup(nativeWindow, containerViewDelegate, this);
    }

    @CalledByNative
    private static AutofillPopupGlue create(int nativeAutofillPopupViewAndroid,
            NativeWindow nativeWindow, ContainerViewDelegate containerViewDelegate) {
        return new AutofillPopupGlue(nativeAutofillPopupViewAndroid, nativeWindow,
                containerViewDelegate);
    }

    @Override
    public void requestHide() {
        nativeRequestHide(mNativeAutofillPopup);
    }

    @Override
    public void suggestionSelected(int listIndex) {
        nativeSuggestionSelected(mNativeAutofillPopup, listIndex);
    }

    /**
     * Hides the Autofill Popup and removes its anchor from the ContainerView.
     */
    @CalledByNative
    private void hide() {
        mAutofillPopup.hide();
    }

    /**
     * Shows an Autofill popup with specified suggestions.
     * @param suggestions Autofill suggestions to be displayed.
     */
    @CalledByNative
    private void show(AutofillSuggestion[] suggestions) {
        mAutofillPopup.show(suggestions);
    }

    /**
     * Sets the location and size of the Autofill popup anchor (input field).
     * @param x X coordinate.
     * @param y Y coordinate.
     * @param width The width of the anchor.
     * @param height The height of the anchor.
     */
    @CalledByNative
    private void setAnchorRect(float x, float y, float width, float height) {
        mAutofillPopup.setAnchorRect(x, y, width, height);
    }

    /**
     * Creates an Autofill Suggestion object with specified name and label.
     * @param name Name of the suggestion.
     * @param label Label of the suggestion.
     * @param uniqueId Unique suggestion id.
     * @return The AutofillSuggestion object with the specified data.
     */
    @CalledByNative
    private static AutofillSuggestion createAutofillSuggestion(String name, String label,
            int uniqueId) {
        return new AutofillSuggestion(name, label, uniqueId);
    }

    private native void nativeRequestHide(int nativeAutofillPopupViewAndroid);
    private native void nativeSuggestionSelected(int nativeAutofillPopupViewAndroid,
            int listIndex);

}