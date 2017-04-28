// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.support.annotation.StringRes;
import android.view.View;
import android.widget.PopupWindow.OnDismissListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement_tracker.FeatureEngagementTrackerFactory;
import org.chromium.chrome.browser.infobar.InfoBarContainer.InfoBarContainerObserver;
import org.chromium.chrome.browser.infobar.InfoBarContainerLayout.Item;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.chrome.browser.widget.textbubble.ViewAnchoredTextBubble;
import org.chromium.components.feature_engagement_tracker.EventConstants;
import org.chromium.components.feature_engagement_tracker.FeatureConstants;
import org.chromium.components.feature_engagement_tracker.FeatureEngagementTracker;

/**
 * A helper class to managing showing and dismissing in-product help dialogs based on which infobar
 * is frontmost and showing.  This will show an in-product help window when a new relevant infobar
 * becomes front-most.  If that infobar is closed or another infobar comes to the front the window
 * will be dismissed.
 */
class IPHInfoBarSupport implements OnDismissListener, InfoBarContainer.InfoBarAnimationListener,
                                   InfoBarContainerObserver {
    private final Context mContext;
    private final FeatureEngagementTracker mTracker;

    /** Helper class to hold all relevant display parameters for an in-product help window. */
    private static class TrackerParameters {
        public TrackerParameters(String feature, @StringRes int textId) {
            this.feature = feature;
            this.textId = textId;
        }

        /** @see FeatureConstants */
        public String feature;

        @StringRes
        public int textId;
    }

    /** Helper class to manage state relating to a particular instance of an in-product window. */
    private static class PopupState {
        /** The View that represents the infobar that the in-product window is attached to. */
        public View view;

        /** The bubble that is currently showing the in-product help. */
        public TextBubble bubble;
    }

    /** The state of the currently showing in-product window or {@code null} if none is showing. */
    private PopupState mCurrentState;

    /** Creates a new instance of an IPHInfoBarSupport class. */
    IPHInfoBarSupport(Context context) {
        mContext = context;
        Profile profile = Profile.getLastUsedProfile();
        mTracker = FeatureEngagementTrackerFactory.getFeatureEngagementTrackerForProfile(profile);
    }

    // InfoBarContainer.InfoBarAnimationListener implementation.
    @Override
    public void notifyAnimationFinished(int animationType) {}

    @Override
    public void notifyAllAnimationsFinished(Item frontInfoBar) {
        View view = frontInfoBar == null ? null : frontInfoBar.getView();

        if (mCurrentState != null) {
            // Clean up any old infobar if necessary.
            if (mCurrentState.view != view) {
                mCurrentState.bubble.dismiss();
                mCurrentState = null;
            }
        }

        if (frontInfoBar == null) return;

        // Check if we need to log any IPH events based on the infobar.
        logEvent(frontInfoBar);

        // Check if there are any IPH'es we need to show.
        TrackerParameters params = getTrackerParameters(frontInfoBar);
        if (params == null) return;

        if (!mTracker.shouldTriggerHelpUI(params.feature)) return;

        mCurrentState = new PopupState();
        mCurrentState.view = view;
        mCurrentState.bubble = new ViewAnchoredTextBubble(mContext, view, params.textId);
        mCurrentState.bubble.addOnDismissListener(this);
        mCurrentState.bubble.setDismissOnTouchInteraction(true);
        mCurrentState.bubble.show();
    }

    // InfoBarContainerObserver implementation.
    @Override
    public void onAddInfoBar(InfoBarContainer container, InfoBar infoBar, boolean isFirst) {}

    @Override
    public void onRemoveInfoBar(InfoBarContainer container, InfoBar infoBar, boolean isLast) {
        if (mCurrentState != null && infoBar.getView() == mCurrentState.view) {
            mCurrentState.bubble.dismiss();
            mCurrentState = null;
        }
    }

    @Override
    public void onInfoBarContainerAttachedToWindow(boolean hasInfobars) {}

    // PopupWindow.OnDismissListener implementation.
    @Override
    public void onDismiss() {
        mCurrentState = null;
        mTracker.dismissed();
    }

    private void logEvent(Item infoBar) {
        switch (infoBar.getInfoBarIdentifier()) {
            case InfoBarIdentifier.DATA_REDUCTION_PROXY_PREVIEW_INFOBAR_DELEGATE:
                mTracker.notifyEvent(EventConstants.DATA_SAVER_PREVIEW_INFOBAR_SHOWN);
                break;
            default:
                break;
        }
    }

    private TrackerParameters getTrackerParameters(Item infoBar) {
        switch (infoBar.getInfoBarIdentifier()) {
            case InfoBarIdentifier.DATA_REDUCTION_PROXY_PREVIEW_INFOBAR_DELEGATE:
                return new TrackerParameters(
                        FeatureConstants.DATA_SAVER_PREVIEW, R.string.iph_data_saver_preview_text);
            default:
                return null;
        }
    }
}