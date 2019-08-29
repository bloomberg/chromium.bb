// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantTagsForTesting;
import org.chromium.chrome.browser.autofill_assistant.AssistantTextUtils;

/**
 * The third party terms and conditions section of the Autofill Assistant payment request.
 */
public class AssistantTermsSection {
    interface Delegate {
        void onStateChanged(@AssistantTermsAndConditionsState int state);

        void onLinkClicked(int link);
    }

    private final View mView;
    private final AssistantChoiceList mTermsList;
    private final TextView mTermsAgree;
    @Nullable
    private final TextView mTermsRequiresReview;
    private final TextView mThirdPartyPrivacyNotice;
    @Nullable
    private Delegate mDelegate;

    private String mOrigin = "";
    private String mAcceptTermsText = "";

    AssistantTermsSection(Context context, ViewGroup parent, boolean showAsSingleCheckbox) {
        mView = LayoutInflater.from(context).inflate(
                R.layout.autofill_assistant_payment_request_terms_and_conditions, parent, false);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        parent.addView(mView, lp);
        mTermsList = mView.findViewById(R.id.third_party_terms_list);

        // This will display the terms as a checkbox instead of radio buttons.
        mTermsList.setAllowMultipleChoices(showAsSingleCheckbox);

        mTermsAgree = new TextView(context);
        ApiCompatibilityUtils.setTextAppearance(mTermsAgree, R.style.TextAppearance_BlackCaption);
        mTermsAgree.setGravity(Gravity.CENTER_VERTICAL);
        mTermsList.addItem(mTermsAgree, /*hasEditButton=*/false, selected -> {
            if (selected) {
                if (mDelegate != null) {
                    mDelegate.onStateChanged(AssistantTermsAndConditionsState.ACCEPTED);
                }
            } else if (showAsSingleCheckbox && mDelegate != null) {
                mDelegate.onStateChanged(AssistantTermsAndConditionsState.NOT_SELECTED);
            }
        }, /* itemEditedListener=*/null);

        if (showAsSingleCheckbox) {
            mTermsRequiresReview = null;
        } else {
            mTermsRequiresReview = new TextView(context);
            ApiCompatibilityUtils.setTextAppearance(
                    mTermsRequiresReview, R.style.TextAppearance_BlackCaption);
            mTermsRequiresReview.setGravity(Gravity.CENTER_VERTICAL);
            mTermsRequiresReview.setTag(
                    AssistantTagsForTesting.COLLECT_USER_DATA_TERMS_REQUIRE_REVIEW);
            mTermsList.addItem(mTermsRequiresReview, /*hasEditButton=*/false, selected -> {
                if (selected && mDelegate != null) {
                    mDelegate.onStateChanged(AssistantTermsAndConditionsState.REQUIRES_REVIEW);
                }
            }, /* itemEditedListener=*/null);
        }

        mThirdPartyPrivacyNotice =
                mView.findViewById(R.id.payment_request_3rd_party_privacy_notice);
    }

    public void setOrigin(String origin) {
        mOrigin = origin;

        setAcceptTermsText();
        Context context = mView.getContext();
        if (mTermsRequiresReview != null) {
            AssistantTextUtils.applyVisualAppearanceTags(mTermsRequiresReview,
                    context.getString(R.string.autofill_assistant_3rd_party_terms_review, origin),
                    /* linkCallback= */ null);
        }
        AssistantTextUtils.applyVisualAppearanceTags(mThirdPartyPrivacyNotice,
                context.getString(R.string.autofill_assistant_3rd_party_privacy_notice, origin),
                /* linkCallback= */ null);
    }

    private void setAcceptTermsText() {
        AssistantTextUtils.applyVisualAppearanceTags(mTermsAgree,
                String.format(mAcceptTermsText, mOrigin), this::onTermsAndConditionsLinkClicked);
    }

    private void onTermsAndConditionsLinkClicked(int link) {
        if (mDelegate != null) {
            mDelegate.onLinkClicked(link);
        }
    }

    public void setTermsStatus(@AssistantTermsAndConditionsState int status) {
        switch (status) {
            case AssistantTermsAndConditionsState.NOT_SELECTED:
                mTermsList.setCheckedItem(null);
                break;
            case AssistantTermsAndConditionsState.ACCEPTED:
                mTermsList.setCheckedItem(mTermsAgree);
                break;
            case AssistantTermsAndConditionsState.REQUIRES_REVIEW:
                if (mTermsRequiresReview != null) {
                    mTermsList.setCheckedItem(mTermsRequiresReview);
                }
                break;
        }
    }

    public void setPaddings(int topPadding, int bottomPadding) {
        mView.setPadding(
                mView.getPaddingLeft(), topPadding, mView.getPaddingRight(), bottomPadding);
    }

    public void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }

    void setAcceptTermsAndConditionsText(String text) {
        if (text.isEmpty()) {
            mTermsList.setVisibility(View.GONE);
        } else {
            mTermsList.setVisibility(View.VISIBLE);
            mAcceptTermsText = text;
            setAcceptTermsText();
        }
    }

    View getView() {
        return mView;
    }
}
