// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCategoryStatus;
import org.chromium.chrome.browser.signin.AccountSigninActivity;
import org.chromium.chrome.browser.signin.SigninAccessPoint;

/**
 * Card that is shown when the user needs to be made aware of some information about their
 * configuration about the NTP suggestions: there is no more available suggested content, sync
 * should be enabled, etc.
 */
public abstract class StatusListItem implements NewTabPageListItem {
    /**
     * ViewHolder for an item of type {@link #VIEW_TYPE_STATUS}.
     */
    public static class ViewHolder extends CardViewHolder {
        private final TextView mTitleView;
        private final TextView mBodyView;
        private final Button mActionView;

        public ViewHolder(NewTabPageRecyclerView parent, UiConfig config) {
            super(R.layout.new_tab_page_status_card, parent, config);
            mTitleView = (TextView) itemView.findViewById(R.id.status_title);
            mBodyView = (TextView) itemView.findViewById(R.id.status_body);
            mActionView = (Button) itemView.findViewById(R.id.status_action_button);
        }

        @Override
        public void onBindViewHolder(NewTabPageListItem item) {
            assert item instanceof StatusListItem;
            super.onBindViewHolder(item);

            final StatusListItem listItem = (StatusListItem) item;
            mTitleView.setText(listItem.mHeaderStringId);
            mBodyView.setText(listItem.mDescriptionStringId);

            if (listItem.hasAction()) {
                mActionView.setText(listItem.mActionStringId);
                mActionView.setOnClickListener(new OnClickListener() {

                    @Override
                    public void onClick(View v) {
                        listItem.performAction(v.getContext());
                    }
                });
                mActionView.setVisibility(View.VISIBLE);
            } else {
                mActionView.setVisibility(View.GONE);
            }
        }
    }

    private static class ErrorListItem extends StatusListItem {
        public ErrorListItem(int headerStringId, int descriptionStringId) {
            super(headerStringId, descriptionStringId, 0);
        }
        @Override
        protected void performAction(Context context) {
            // No action.
        }

        @Override
        protected boolean hasAction() {
            return false;
        }
    }

    private static class NoSnippets extends StatusListItem {
        private final NewTabPageAdapter mNewTabPageAdapter;

        public NoSnippets(NewTabPageAdapter adapter) {
            super(R.string.ntp_status_card_title_empty,
                    R.string.ntp_status_card_body_empty,
                    R.string.reload);
            mNewTabPageAdapter = adapter;
            Log.d(TAG, "Registering card for status: No Snippets");
        }

        @Override
        protected void performAction(Context context) {
            mNewTabPageAdapter.reloadSnippets();
        }
    }

    private static class SignedOut extends StatusListItem {
        public SignedOut() {
            super(R.string.snippets_disabled_generic_prompt,
                    R.string.snippets_disabled_signed_out_instructions,
                    R.string.sign_in_button);
            Log.d(TAG, "Registering card for status: User Signed out");
        }

        @Override
        protected void performAction(Context context) {
            AccountSigninActivity.startIfAllowed(context, SigninAccessPoint.NTP_LINK);
        }
    }

    private static class CategoryExplicitlyDisabled extends ErrorListItem {
        public CategoryExplicitlyDisabled() {
            // TODO(pke): Those are technically the wrong strings, but they roughly fit in this
            // case. This should only be called when the category has been disabled via enterprise
            // policy. Replace this with proper error card once it has been specified.
            super(R.string.ntp_status_card_title_empty, R.string.ntp_status_card_body_empty);
            Log.d(TAG, "Registering card for error: Category Explicitly Disabled");
        }
    }

    private static class ProviderError extends ErrorListItem {
        public ProviderError() {
            // TODO(pke): Those are technically the wrong strings, but they roughly fit in this
            // case. This is only called if NTPSnippetsDatabase encounters an error.
            // Replace this with proper error card once it has been specified.
            super(R.string.ntp_status_card_title_empty, R.string.ntp_status_card_body_empty);
            Log.d(TAG, "Registering card for error: Provider Error");
        }
    }

    private static final String TAG = "NtpCards";

    private final int mHeaderStringId;
    private final int mDescriptionStringId;
    private final int mActionStringId;

    public static StatusListItem create(int categoryStatus, NewTabPageAdapter adapter) {
        switch (categoryStatus) {
            // TODO(dgn): AVAILABLE_LOADING and INITIALIZING should show a progress indicator.
            case ContentSuggestionsCategoryStatus.AVAILABLE:
            case ContentSuggestionsCategoryStatus.AVAILABLE_LOADING:
            case ContentSuggestionsCategoryStatus.INITIALIZING:
                return new NoSnippets(adapter);

            case ContentSuggestionsCategoryStatus.SIGNED_OUT:
                return new SignedOut();

            case ContentSuggestionsCategoryStatus.ALL_SUGGESTIONS_EXPLICITLY_DISABLED:
                Log.wtf(TAG, "FATAL: Attempted to create a status card while the feature should be "
                        + "off.");
                return null;

            case ContentSuggestionsCategoryStatus.CATEGORY_EXPLICITLY_DISABLED:
                Log.d(TAG, "Not showing ARTICLES suggestions because this category is disabled.");
                // TODO(pke): Replace this.
                return new CategoryExplicitlyDisabled();

            case ContentSuggestionsCategoryStatus.NOT_PROVIDED:
                Log.wtf(TAG, "FATAL: Attempted to create a status card for content suggestions "
                                + " when provider for ARTICLES is not registered.");
                return null;

            case ContentSuggestionsCategoryStatus.LOADING_ERROR:
                Log.d(TAG, "Not showing ARTICLES suggestions because of provider error.");
                // TODO(pke): Replace this.
                return new ProviderError();

            default:
                Log.wtf(TAG, "FATAL: Attempted to create a status card for an unknown value: %d",
                        categoryStatus);
                return null;
        }
    }

    private StatusListItem(int headerStringId, int descriptionStringId, int actionStringId) {
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
        return NewTabPageListItem.VIEW_TYPE_STATUS;
    }
}
