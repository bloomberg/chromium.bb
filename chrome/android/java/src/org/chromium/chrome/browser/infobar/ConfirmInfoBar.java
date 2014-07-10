// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

/**
 * An infobar that presents the user with several buttons.
 *
 * TODO(newt): merge this into InfoBar.java.
 */
public class ConfirmInfoBar extends InfoBar {
    /** Text shown on the primary button, e.g. "OK". */
    private final String mPrimaryButtonText;

    /** Text shown on the secondary button, e.g. "Cancel".*/
    private final String mSecondaryButtonText;

    /** Text shown on the extra button, e.g. "More info". */
    private final String mTertiaryButtonText;

    /** Notified when one of the buttons is clicked. */
    private final InfoBarListeners.Confirm mConfirmListener;

    public ConfirmInfoBar(long nativeInfoBar, InfoBarListeners.Confirm confirmListener,
            int iconDrawableId, String message, String linkText, String primaryButtonText,
            String secondaryButtonText) {
        super(confirmListener, iconDrawableId, message);
        mPrimaryButtonText = primaryButtonText;
        mSecondaryButtonText = secondaryButtonText;
        mTertiaryButtonText = linkText;
        mConfirmListener = confirmListener;
        setNativeInfoBar(nativeInfoBar);
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        layout.setButtons(mPrimaryButtonText, mSecondaryButtonText, mTertiaryButtonText);
    }

    @Override
    public void onButtonClicked(boolean isPrimaryButton) {
        if (mConfirmListener != null) {
            mConfirmListener.onConfirmInfoBarButtonClicked(this, isPrimaryButton);
        }

        if (mNativeInfoBarPtr != 0) {
            int action = isPrimaryButton ? InfoBar.ACTION_TYPE_OK : InfoBar.ACTION_TYPE_CANCEL;
            nativeOnButtonClicked(mNativeInfoBarPtr, action, "");
        }
    }

    @Override
    public void onCloseButtonClicked() {
        if (mNativeInfoBarPtr != 0) {
            nativeOnCloseButtonClicked(mNativeInfoBarPtr);
        } else {
            super.dismissJavaOnlyInfoBar();
        }
    }
}
