// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.widget.CheckBox;

import org.chromium.chrome.browser.password_manager.PasswordAuthenticationManager;

/**
 * The infobar that allows saving passwords for autofill.
 */
public class SavePasswordInfoBar extends ConfirmInfoBar {

    private final SavePasswordInfoBarDelegate mDelegate;
    private final long mNativeInfoBar;
    private CheckBox mUseAdditionalAuthenticationCheckbox;

    public SavePasswordInfoBar(long nativeInfoBar, SavePasswordInfoBarDelegate delegate,
            int iconDrawableId, String message, String primaryButtonText,
            String secondaryButtonText) {
        super(nativeInfoBar, null, iconDrawableId, message,
                null, primaryButtonText, secondaryButtonText);
        mNativeInfoBar = nativeInfoBar;
        mDelegate = delegate;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        if (PasswordAuthenticationManager.isPasswordAuthenticationEnabled()) {
            mUseAdditionalAuthenticationCheckbox = new CheckBox(getContext());
            mUseAdditionalAuthenticationCheckbox.setText(
                    PasswordAuthenticationManager.getPasswordProtectionString());
            layout.setCustomContent(mUseAdditionalAuthenticationCheckbox);
        }

        super.createContent(layout);
    }

    @Override
    public void onButtonClicked(boolean isPrimaryButton) {
        if (isPrimaryButton && mUseAdditionalAuthenticationCheckbox != null
                && mUseAdditionalAuthenticationCheckbox.isChecked()) {
            mDelegate.setUseAdditionalAuthentication(mNativeInfoBar, true);
        }
        super.onButtonClicked(isPrimaryButton);
    }
}
