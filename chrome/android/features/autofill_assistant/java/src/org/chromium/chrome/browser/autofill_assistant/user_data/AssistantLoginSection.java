// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_AUTO;
import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO;

import android.content.Context;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.ui.UiUtils;

import java.util.List;

/**
 * The login details section of the Autofill Assistant payment request.
 */
public class AssistantLoginSection extends AssistantCollectUserDataSection<AssistantLoginChoice> {
    AssistantLoginSection(Context context, ViewGroup parent) {
        super(context, parent, R.layout.autofill_assistant_login, R.layout.autofill_assistant_login,
                context.getResources().getDimensionPixelSize(
                        org.chromium.chrome.autofill_assistant.R.dimen
                                .autofill_assistant_payment_request_title_padding),
                /*titleAddButton=*/null, /*listAddButton=*/null);
    }

    @Override
    protected void createOrEditItem(@Nullable AssistantLoginChoice oldItem) {
        assert oldItem != null;
        assert oldItem.getInfoPopup() != null;

        new UiUtils.CompatibleAlertDialogBuilder(mContext, R.style.Theme_Chromium_AlertDialog)
                .setTitle(oldItem.getInfoPopup().getTitle())
                .setMessage(oldItem.getInfoPopup().getText())
                .setPositiveButton(R.string.close, (dialog, which) -> {})
                .show();
    }

    @Override
    protected void updateFullView(View fullView, AssistantLoginChoice option) {
        updateSummaryView(fullView, option);
    }

    @Override
    protected void updateSummaryView(View summaryView, AssistantLoginChoice option) {
        TextView labelView = summaryView.findViewById(R.id.label);
        labelView.setText(option.getLabel());
        TextView sublabelView = summaryView.findViewById(R.id.sublabel);
        if (TextUtils.isEmpty(option.getSublabel())) {
            sublabelView.setVisibility(View.GONE);
        } else {
            sublabelView.setText(option.getSublabel());
            sublabelView.setContentDescription(option.getSublabelAccessibilityHint());
            sublabelView.setImportantForAccessibility(
                    TextUtils.isEmpty(option.getSublabelAccessibilityHint())
                            ? IMPORTANT_FOR_ACCESSIBILITY_NO
                            : IMPORTANT_FOR_ACCESSIBILITY_AUTO);
        }
    }

    @Override
    protected boolean canEditOption(AssistantLoginChoice choice) {
        return choice.getInfoPopup() != null;
    }

    @Override
    protected @DrawableRes int getEditButtonDrawable(AssistantLoginChoice choice) {
        return R.drawable.btn_info;
    }

    @Override
    protected String getEditButtonContentDescription(AssistantLoginChoice choice) {
        // TODO(b/143862732): Send this a11y string from the backend.
        return mContext.getString(R.string.learn_more);
    }

    /**
     * The login options have changed externally. This will rebuild the UI with the new/changed
     * set of login options, while keeping the selected item if possible.
     */
    void onLoginsChanged(List<AssistantLoginChoice> options) {
        int indexToSelect = -1;
        if (mSelectedOption != null) {
            for (int i = 0; i < getItems().size(); i++) {
                if (TextUtils.equals(
                            mSelectedOption.getIdentifier(), getItems().get(i).getIdentifier())) {
                    indexToSelect = i;
                    break;
                }
            }
        }

        // Preselect login option according to priority. This will implicitly select the first
        // option if all options have the same (default) priority.
        if (indexToSelect == -1) {
            int highestPriority = Integer.MAX_VALUE;
            for (int i = 0; i < options.size(); i++) {
                int priority = options.get(i).getPriority();
                if (priority < highestPriority) {
                    highestPriority = priority;
                    indexToSelect = i;
                }
            }
        }

        setItems(options, indexToSelect);
    }
}
