// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.ResourceId;

/**
 * The Save Password infobar offers the user the ability to save a password for the site.
 * Appearance and behaviour of infobar buttons depends on from where infobar was
 * triggered.
 */
public class SavePasswordInfoBar extends ConfirmInfoBar {
    private final boolean mIsMoreButtonNeeded;

    @CalledByNative
    private static InfoBar show(long nativeInfoBar, int enumeratedIconId, String message,
            String primaryButtonText, String secondaryButtonText, boolean isMoreButtonNeeded) {
        return new SavePasswordInfoBar(nativeInfoBar, ResourceId.mapToDrawableId(enumeratedIconId),
                message, primaryButtonText, secondaryButtonText, isMoreButtonNeeded);
    }

    private SavePasswordInfoBar(long nativeInfoBar, int iconDrawbleId, String message,
            String primaryButtonText, String secondaryButtonText, boolean isMoreButtonNeeded) {
        super(nativeInfoBar, null, iconDrawbleId, null, message, null, primaryButtonText,
                secondaryButtonText);
        mIsMoreButtonNeeded = isMoreButtonNeeded;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        if (mIsMoreButtonNeeded) {
            layout.setCustomViewInButtonRow(OverflowSelector.createOverflowSelector(getContext()));
        }
    }
}
