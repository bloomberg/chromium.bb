// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;

/**
 * After user proceed through Safe Browsing warning interstitials that are displayed when the site
 * ahead contains deceptive embedded content, the infobar appears, it explains the user that some
 * subresources were filtered and presents the "Detals" link. If the link is pressed full infobar
 * with detailed text and the action buttons appears, it gives the user an ability to reload the
 * page with the content we've blocked previously.
 */
public class SubresourceFilterExperimentalInfoBar extends ConfirmInfoBar {
    private final String mMessage;
    private final String mFollowUpMessage;
    private final String mOKButtonText;
    private final String mReloadButtonText;
    private boolean mShowExplanation;

    @CalledByNative
    private static InfoBar show(int enumeratedIconId, String message, String oKButtonText,
            String reloadButtonText, String followUpMessage) {
        return new SubresourceFilterExperimentalInfoBar(
                ResourceId.mapToDrawableId(enumeratedIconId), message, oKButtonText,
                reloadButtonText, followUpMessage);
    }

    private SubresourceFilterExperimentalInfoBar(int iconDrawbleId, String message,
            String oKButtonText, String reloadButtonText, String followUpMessage) {
        super(iconDrawbleId, null, message, null, null, null); //, oKButtonText, reloadButtonText);
        mFollowUpMessage = followUpMessage;
        mMessage = message;
        mOKButtonText = oKButtonText;
        mReloadButtonText = reloadButtonText;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        if (mShowExplanation) {
            layout.setMessage(mFollowUpMessage);
            setButtons(layout, mOKButtonText, mReloadButtonText);
        } else {
            String link = layout.getContext().getString(R.string.details_link);
            layout.setMessage(mMessage);
            layout.setMessageLinkText(link);
        }
    }

    @Override
    public void onLinkClicked() {
        mShowExplanation = true;
        replaceView(createView());
    }
}
