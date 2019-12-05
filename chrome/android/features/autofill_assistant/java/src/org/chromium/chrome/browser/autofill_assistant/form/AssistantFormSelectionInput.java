// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.TextView;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantTextUtils;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantChoiceList;

import java.util.List;

/** A form input that allows to choose between multiple choices. */
class AssistantFormSelectionInput extends AssistantFormInput {
    interface Delegate {
        void onChoiceSelectionChanged(int choiceIndex, boolean selected);
        void onLinkClicked(int link);
    }

    private final String mLabel;
    private final List<AssistantFormSelectionChoice> mChoices;
    private final boolean mAllowMultipleChoices;
    private final Delegate mDelegate;

    public AssistantFormSelectionInput(String label, List<AssistantFormSelectionChoice> choices,
            boolean allowMultipleChoices, Delegate delegate) {
        mLabel = label;
        mChoices = choices;
        mAllowMultipleChoices = allowMultipleChoices;
        mDelegate = delegate;
    }

    @Override
    public View createView(Context context, ViewGroup parent) {
        LayoutInflater inflater = LayoutInflater.from(context);
        ViewGroup root = (ViewGroup) inflater.inflate(
                R.layout.autofill_assistant_form_selection_input, parent,
                /* attachToRoot= */ false);
        TextView label = root.findViewById(org.chromium.chrome.autofill_assistant.R.id.label);
        if (mLabel.isEmpty()) {
            label.setVisibility(View.GONE);
        } else {
            AssistantTextUtils.applyVisualAppearanceTags(label, mLabel, mDelegate::onLinkClicked);
        }

        if (mChoices.isEmpty()) {
            return root;
        }

        ViewGroup checkboxList = root.findViewById(R.id.checkbox_list);
        AssistantChoiceList radiobuttonList = root.findViewById(R.id.radiobutton_list);
        for (int i = 0; i < mChoices.size(); i++) {
            AssistantFormSelectionChoice choice = mChoices.get(i);

            int index = i; // needed for the lambda.
            View choiceView;
            if (mAllowMultipleChoices) {
                choiceView = inflater.inflate(R.layout.autofill_assistant_form_checkbox,
                        checkboxList, /* attachToRoot= */ false);
                checkboxList.addView(choiceView);

                CheckBox checkBox = choiceView.findViewById(R.id.checkbox);
                checkBox.setOnCheckedChangeListener(
                        (compoundButton,
                                isChecked) -> mDelegate.onChoiceSelectionChanged(index, isChecked));
                choiceView.findViewById(R.id.descriptions)
                        .setOnClickListener(
                                unusedView -> checkBox.setChecked(!checkBox.isChecked()));
                checkBox.setChecked(choice.isInitiallySelected());
            } else {
                choiceView = inflater.inflate(R.layout.autofill_assistant_form_radiobutton, null);

                radiobuttonList.addItem(choiceView, /* hasEditButton= */ false,
                        (isChecked)
                                -> mDelegate.onChoiceSelectionChanged(index, isChecked),
                        /* itemEditedListener= */ null);
                radiobuttonList.setChecked(choiceView, choice.isInitiallySelected());
            }

            TextView choiceLabel = choiceView.findViewById(R.id.label);
            TextView descriptionLine1 = choiceView.findViewById(R.id.description_line_1);
            TextView descriptionLine2 = choiceView.findViewById(R.id.description_line_2);
            AssistantTextUtils.applyVisualAppearanceTags(
                    choiceLabel, choice.getLabel(), mDelegate::onLinkClicked);
            AssistantTextUtils.applyVisualAppearanceTags(
                    descriptionLine1, choice.getDescriptionLine1(), mDelegate::onLinkClicked);
            AssistantTextUtils.applyVisualAppearanceTags(
                    descriptionLine2, choice.getDescriptionLine2(), mDelegate::onLinkClicked);
            hideIfEmpty(choiceLabel);
            hideIfEmpty(descriptionLine1);
            hideIfEmpty(descriptionLine2);
            setMinimumHeight(choiceView, descriptionLine1, descriptionLine2);
        }
        if (mAllowMultipleChoices) {
            checkboxList.setVisibility(View.VISIBLE);
        } else {
            radiobuttonList.setVisibility(View.VISIBLE);
        }
        return root;
    }
}
