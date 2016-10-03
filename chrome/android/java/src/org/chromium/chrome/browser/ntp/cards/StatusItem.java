// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.Context;
import android.support.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus.CategoryStatusEnum;

/**
 * Card that is shown when the user needs to be made aware of some information about their
 * configuration about the NTP suggestions: there is no more available suggested content, sync
 * should be enabled, etc.
 */
public abstract class StatusItem implements NewTabPageItem {
    /**
     * Delegate to provide the status cards a way to interact with the rest of the system: tap
     * handler, etc.
     */
    public interface ActionDelegate { void onButtonTapped(); }

    private static class NoBookmarks extends StatusItem {
        public NoBookmarks() {
            super(R.string.ntp_status_card_title_no_bookmarks,
                    R.string.ntp_status_card_no_bookmarks, 0);
            Log.d(TAG, "Registering card for status: No Bookmarks");
        }

        @Override
        protected void performAction(Context context) {
            assert false; // not reached.
        }

        @Override
        protected boolean hasAction() {
            return false;
        }
    }

    private static class NoSnippets extends StatusItem {
        private final ActionDelegate mActionDelegate;

        public NoSnippets(ActionDelegate actionDelegate) {
            super(R.string.ntp_status_card_title_no_articles, R.string.ntp_status_card_no_articles,
                    R.string.reload);
            mActionDelegate = actionDelegate;
            Log.d(TAG, "Registering card for status: No Snippets");
        }

        @Override
        protected void performAction(Context context) {
            mActionDelegate.onButtonTapped();
        }
    }

    private static final String TAG = "NtpCards";

    private final int mHeaderStringId;
    private final int mDescriptionStringId;
    private final int mActionStringId;

    public static StatusItem create(
            @CategoryStatusEnum int categoryStatus, @Nullable ActionDelegate actionDelegate) {
        switch (categoryStatus) {
            case CategoryStatus.SIGNED_OUT:
            // Fall through. Sign out is a transitive state that we just use to clear content.
            case CategoryStatus.AVAILABLE:
            case CategoryStatus.AVAILABLE_LOADING:
            case CategoryStatus.INITIALIZING:
                // TODO(dgn): rewrite this whole thing? Get one card and change its state instead
                // of recreating it. It would be more flexible in terms of adapting the content
                // to different usages.
                return actionDelegate == null ? new NoBookmarks() : new NoSnippets(actionDelegate);

            case CategoryStatus.ALL_SUGGESTIONS_EXPLICITLY_DISABLED:
                Log.wtf(TAG, "Attempted to create a status card while the feature should be off.");
                return null;

            case CategoryStatus.CATEGORY_EXPLICITLY_DISABLED:
                // In this case, the entire section should have been cleared off the UI.
                Log.wtf(TAG, "Attempted to create a status card for content suggestions "
                                + " when the category status is CATEGORY_EXPLICITLY_DISABLED.");
                return null;

            case CategoryStatus.NOT_PROVIDED:
                // In this case, the UI should remain as it is and also keep the previous category
                // status, so the NOT_PROVIDED should never reach here.
                Log.wtf(TAG, "Attempted to create a status card for content suggestions "
                                + " when the category is NOT_PROVIDED.");
                return null;

            case CategoryStatus.LOADING_ERROR:
                // In this case, the entire section should have been cleared off the UI.
                Log.wtf(TAG, "Attempted to create a status card for content suggestions "
                                + " when the category is LOADING_ERROR.");
                return null;

            default:
                Log.wtf(TAG, "Attempted to create a status card for an unknown value: %d",
                        categoryStatus);
                return null;
        }
    }

    protected StatusItem(int headerStringId, int descriptionStringId, int actionStringId) {
        mHeaderStringId = headerStringId;
        mDescriptionStringId = descriptionStringId;
        mActionStringId = actionStringId;
    }

    protected abstract void performAction(Context context);

    protected boolean hasAction() {
        return true;
    }

    @Override
    public int getType() {
        return NewTabPageItem.VIEW_TYPE_STATUS;
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder) {
        assert holder instanceof StatusCardViewHolder;
        ((StatusCardViewHolder) holder).onBindViewHolder(this);
    }

    public int getHeader() {
        return mHeaderStringId;
    }

    public int getDescription() {
        return mDescriptionStringId;
    }

    public int getActionLabel() {
        return mActionStringId;
    }
}
