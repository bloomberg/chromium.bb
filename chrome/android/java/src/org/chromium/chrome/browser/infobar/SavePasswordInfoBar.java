// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.ResourceId;

/**
 * The Save Password infobar offers the user the ability to save a password for the site.
 * Appearance and behaviour of infobar buttons depends on from where infobar was
 * triggered.
 */
public class SavePasswordInfoBar extends ConfirmInfoBar {
    private final boolean mIsMoreButtonNeeded;
    private final int mTitleLinkRangeStart;
    private final int mTitleLinkRangeEnd;
    private final String mTitle;

    @CalledByNative
    private static InfoBar show(long nativeInfoBar, int enumeratedIconId, String message,
            int titleLinkStart, int titleLinkEnd, String primaryButtonText,
            String secondaryButtonText, boolean isMoreButtonNeeded) {
        return new SavePasswordInfoBar(nativeInfoBar, ResourceId.mapToDrawableId(enumeratedIconId),
                message, titleLinkStart, titleLinkEnd, primaryButtonText, secondaryButtonText,
                isMoreButtonNeeded);
    }

    private SavePasswordInfoBar(long nativeInfoBar, int iconDrawbleId, String message,
            int titleLinkStart, int titleLinkEnd, String primaryButtonText,
            String secondaryButtonText, boolean isMoreButtonNeeded) {
        super(nativeInfoBar, null, iconDrawbleId, null, message, null, primaryButtonText,
                secondaryButtonText);
        mIsMoreButtonNeeded = isMoreButtonNeeded;
        mTitleLinkRangeStart = titleLinkStart;
        mTitleLinkRangeEnd = titleLinkEnd;
        mTitle = message;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        if (mIsMoreButtonNeeded) {
            layout.setCustomViewInButtonRow(OverflowSelector.createOverflowSelector(getContext()));
        }
        if (mTitleLinkRangeStart != 0 && mTitleLinkRangeEnd != 0) {
            SpannableString title = new SpannableString(mTitle);
            title.setSpan(new ClickableSpan() {
                @Override
                public void onClick(View view) {
                    onLinkClicked();
                }
            }, mTitleLinkRangeStart, mTitleLinkRangeEnd, Spanned.SPAN_INCLUSIVE_INCLUSIVE);
            layout.setMessage(title);
        }
    }
}
