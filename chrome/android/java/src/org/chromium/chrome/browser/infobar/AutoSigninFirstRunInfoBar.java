// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.ClickableSpan;
import android.view.View;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;

/**
 * The auto sign-in first run experience infobar is shown instead of usual auto sign-in snackbar
 * when user first time faces the auto sign-in feature. It contains information about which account
 * was used for signing in and text which explains the feature.
 */
public class AutoSigninFirstRunInfoBar extends ConfirmInfoBar {
    private final String mExplanation;
    private final int mExplanationLinkStart;
    private final int mExplanationLinkEnd;

    @CalledByNative
    private static InfoBar show(String message, String primaryButtonText, String explanation,
            int explanationLinkStart, int explanationLinkEnd) {
        return new AutoSigninFirstRunInfoBar(
                message, primaryButtonText, explanation, explanationLinkStart, explanationLinkEnd);
    }

    private AutoSigninFirstRunInfoBar(String message, String primaryButtonText, String explanation,
            int explanationLinkStart, int explanationLinkEnd) {
        super(null, R.drawable.account_management_no_picture, null, message, null,
                primaryButtonText, null);
        mExplanation = explanation;
        mExplanationLinkStart = explanationLinkStart;
        mExplanationLinkEnd = explanationLinkEnd;
    }

    /**
     * Adds text, which explains the auto sign-in feature.
     */
    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        SpannableString explanation = new SpannableString(mExplanation);
        if (mExplanationLinkEnd != mExplanationLinkStart && mExplanationLinkEnd != 0) {
            explanation.setSpan(new ClickableSpan() {
                @Override
                public void onClick(View view) {
                    onLinkClicked();
                }
            }, mExplanationLinkStart, mExplanationLinkEnd, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        }

        InfoBarControlLayout controlLayout = new InfoBarControlLayout(getContext());
        controlLayout.addDescription(explanation);
        layout.setCustomContent(controlLayout);
    }
}
