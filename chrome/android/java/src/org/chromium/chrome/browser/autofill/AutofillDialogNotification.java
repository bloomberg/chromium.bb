// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

/**
 * AutofillDialog notification container.
 */
class AutofillDialogNotification {
    final int mBackgroundColor;
    final int mTextColor;
    final boolean mHasArrow;
    final boolean mHasCheckbox;
    final String mText;

    public AutofillDialogNotification(int backgroundColor, int textColor,
            boolean hasArrow, boolean hasCheckbox, String text) {
        mBackgroundColor = backgroundColor;
        mTextColor = textColor;
        mHasArrow = hasArrow;
        mHasCheckbox = hasCheckbox;
        mText = text;
    }
}
