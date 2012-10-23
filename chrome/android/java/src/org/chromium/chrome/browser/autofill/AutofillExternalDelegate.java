// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.graphics.Rect;

import org.chromium.base.CalledByNative;
import org.chromium.content.browser.ContainerViewDelegate;
import org.chromium.ui.gfx.NativeWindow;

/**
 * Android specific external delegate for display and selection of autofill data.
 */
public class AutofillExternalDelegate {

    // Area of the Autofill popup anchor position used to position the Autofill window.
    private Rect mAutofillAnchorRect;
    private AutofillWindow mAutofillWindow;
    private final int mNativeAutofillExternalDelegate;
    private final ContainerViewDelegate mContainerViewDelegate;

    /**
     * Creates an AutofillExternalDelegate object.
     * @param nativeAutofillExternalDelegate A pointer to the native AutofillExternalDelegate.
     * @param containerViewDelegate A delegate that allows adding and removing views.
     */
    public AutofillExternalDelegate(int nativeAutofillExternalDelegate,
            ContainerViewDelegate containerViewDelegate) {
        mNativeAutofillExternalDelegate = nativeAutofillExternalDelegate;
        mContainerViewDelegate = containerViewDelegate;
    }

    @CalledByNative
    private static AutofillExternalDelegate create(int nativeAutofillExternalDelegate,
            ContainerViewDelegate containerViewDelegate) {
        return new AutofillExternalDelegate(nativeAutofillExternalDelegate, containerViewDelegate);
    }

    /**
     * Sets the autofill window's position in the proper space (window space).
     */
    private void setAutofillWindowPosition() {
        if (mAutofillWindow != null && mAutofillAnchorRect != null) {
            mAutofillWindow.setPositionAround(mAutofillAnchorRect);
        }
    }

    /**
     * Informs the native delegate that an Autofill suggestion has been chosen.
     * @param listIndex The index of the autofill suggestion selected
     * @param value The value of the autofill suggestion.
     * @param uniqueId The unique ID of the autofill suggestion selected.
     */
    public void autofillPopupSelected(int listIndex, String value, int uniqueId) {
        if (mNativeAutofillExternalDelegate != 0) {
            nativeAutofillPopupSelected(
                    mNativeAutofillExternalDelegate, listIndex, value, uniqueId);
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void openAutofillPopup(NativeWindow nativeWindow, AutofillSuggestion[] data) {
        if (mAutofillWindow == null) {
            mAutofillWindow = new AutofillWindow(nativeWindow, mContainerViewDelegate, this, data);
        } else {
            mAutofillWindow.setAutofillSuggestions(data);
        }
        if (mAutofillAnchorRect != null) {
            setAutofillWindowPosition();
            mAutofillWindow.show();
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void closeAutofillPopup() {
        if (mAutofillWindow == null) return;
        mAutofillWindow.dismiss();
        mAutofillWindow = null;
        mAutofillAnchorRect = null;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void setAutofillAnchorRect(float x, float y, float width, float height) {
        mAutofillAnchorRect = new Rect((int) x, (int) y, (int) (x + width), (int) (y + height));
        setAutofillWindowPosition();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private static AutofillSuggestion createAutofillSuggestion(String name, String label,
            int uniqueId) {
        return new AutofillSuggestion(name, label, uniqueId);
    }

    private native void nativeAutofillPopupSelected(int nativeAutofillExternalDelegateAndroid,
            int listIndex, String value, int uniqueId);
}