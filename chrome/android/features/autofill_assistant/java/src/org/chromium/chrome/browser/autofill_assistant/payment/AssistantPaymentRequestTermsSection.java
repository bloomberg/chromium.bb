// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.content.Context;
import android.text.style.StyleSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.ui.text.SpanApplier;

/**
 * The third party terms and conditions section of the Autofill Assistant payment request.
 */
public class AssistantPaymentRequestTermsSection {
    private final View mView;
    private final Callback<Integer> mListener;

    AssistantPaymentRequestTermsSection(
            Context context, ViewGroup parent, String origin, Callback<Integer> listener) {
        mListener = listener;
        mView = LayoutInflater.from(context).inflate(
                R.layout.autofill_assistant_payment_request_terms_and_conditions, parent, false);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        parent.addView(mView, lp);
        AssistantChoiceList list = mView.findViewById(R.id.third_party_terms_list);

        TextView termsAgree = new TextView(context);
        StyleSpan boldSpan = new StyleSpan(android.graphics.Typeface.BOLD);
        termsAgree.setText(SpanApplier.applySpans(
                context.getString(R.string.autofill_assistant_3rd_party_terms_accept, origin),
                new SpanApplier.SpanInfo("<b>", "</b>", boldSpan)));
        ApiCompatibilityUtils.setTextAppearance(
                termsAgree, org.chromium.chrome.R.style.TextAppearance_BlackCaption);
        list.addItem(termsAgree, /*hasEditButton=*/false, selected -> {
            if (selected && mListener != null) {
                mListener.onResult(AssistantTermsAndConditionsState.ACCEPTED);
            }
        }, /* itemEditedListener=*/null);

        TextView termsReview = new TextView(context);
        termsReview.setText(SpanApplier.applySpans(
                context.getString(R.string.autofill_assistant_3rd_party_terms_review, origin),
                new SpanApplier.SpanInfo("<b>", "</b>", boldSpan)));
        ApiCompatibilityUtils.setTextAppearance(
                termsReview, org.chromium.chrome.R.style.TextAppearance_BlackCaption);
        list.addItem(termsReview, /*hasEditButton=*/false, selected -> {
            if (selected && mListener != null) {
                mListener.onResult(AssistantTermsAndConditionsState.REQUIRES_REVIEW);
            }
        }, /* itemEditedListener=*/null);

        // Set 3rd party privacy notice text.
        TextView thirdPartyPrivacyNotice =
                mView.findViewById(R.id.payment_request_3rd_party_privacy_notice);
        thirdPartyPrivacyNotice.setText(SpanApplier.applySpans(
                context.getString(R.string.autofill_assistant_3rd_party_privacy_notice, origin),
                new SpanApplier.SpanInfo("<b>", "</b>", boldSpan)));
    }

    View getView() {
        return mView;
    }
}