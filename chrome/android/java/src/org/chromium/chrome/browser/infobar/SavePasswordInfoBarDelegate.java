// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.ResourceId;

/**
 * Provides JNI methods for SavePasswordInfoBars.
 */
public class SavePasswordInfoBarDelegate {
    private SavePasswordInfoBarDelegate() {
    }

    @CalledByNative
    public static SavePasswordInfoBarDelegate create() {
        return new SavePasswordInfoBarDelegate();
    }

    /**
     * Creates and begins the process for showing a SavePasswordInfoBarDelegate.
     * @param nativeInfoBar Pointer to the C++ InfoBar corresponding to the Java InfoBar.
     * @param enumeratedIconId ID corresponding to the icon that will be shown for the InfoBar.
     *                         The ID must have been mapped using the ResourceMapper class before
     *                         passing it to this function.
     * @param message Message to display to the user indicating what the InfoBar is for.
     * @param buttonOk String to display on the OK button.
     * @param buttonCancel String to display on the Cancel button.
     */
    @CalledByNative
    InfoBar showSavePasswordInfoBar(long nativeInfoBar, int enumeratedIconId, String message,
            String buttonOk, String buttonCancel) {
        int drawableId = ResourceId.mapToDrawableId(enumeratedIconId);
        SavePasswordInfoBar infoBar = new SavePasswordInfoBar(
                nativeInfoBar, this, drawableId, message, buttonOk, buttonCancel);
        return infoBar;
    }

    /**
     * Sets whether additional authentication should be required before this password can be
     * autofilled into a form.
     *
     * @param nativeInfoBar The native infobar pointer.
     * @param useAdditionalAuthencation Whether additional authentication should be required.
     */
    void setUseAdditionalAuthentication(long nativeInfoBar, boolean useAdditionalAuthencation) {
        nativeSetUseAdditionalAuthentication(nativeInfoBar, useAdditionalAuthencation);
    }

    private native void nativeSetUseAdditionalAuthentication(
            long nativeSavePasswordInfoBar, boolean useAdditionalAuthentication);
}
