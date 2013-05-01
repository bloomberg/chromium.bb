// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

/**
 * AutofillDialog notification container.
 */
class AutofillDialogNotification {
    /**
     * The background color for the View's notification area.
     */
    final int mBackgroundColor;

    /**
     * The text color.
     */
    final int mTextColor;

    /**
     * Whether this notification has an arrow pointing up at the account chooser.
     */
    final boolean mHasArrow;

    /**
     * Whether this notifications has the "Save details to wallet" checkbox.
     */
    final boolean mHasCheckbox;

    /**
     * Whether the dialog notification's checkbox should be checked.
     * Only applies when mHasCheckbox is true.
     */
    final boolean mChecked;

    /**
     * When false, this disables user interaction with the notification.
     * For example, WALLET_USAGE_CONFIRMATION notifications set this to false
     * after the submit flow has started.
     */
    final boolean mInteractive;

    /**
     * The text of the notification.
     */
    final String mText;

    /**
     * The (native) type of the notification (see autofill::DialogNotification::Type).
     */
    final int mType;

    public AutofillDialogNotification(
            int type,
            int backgroundColor, int textColor,
            boolean hasArrow, boolean hasCheckbox,
            boolean checked, boolean interactive,
            String text) {
        mType = type;
        mBackgroundColor = backgroundColor;
        mTextColor = textColor;
        mHasArrow = hasArrow;
        mHasCheckbox = hasCheckbox;
        mChecked = checked;
        mInteractive = interactive;
        mText = text;
    }
}
