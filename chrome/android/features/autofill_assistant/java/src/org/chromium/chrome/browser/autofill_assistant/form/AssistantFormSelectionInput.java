// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;

import java.util.List;

/** A form input that allows to choose between multiple choices. */
class AssistantFormSelectionInput extends AssistantFormInput {
    interface Delegate {
        void onChoiceSelectionChanged(int choiceIndex, boolean selected);
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
        ViewGroup root = (ViewGroup) LayoutInflater.from(context).inflate(
                org.chromium.chrome.autofill_assistant.R.layout
                        .autofill_assistant_form_selection_input,
                parent, /* attachToRoot= */ false);
        TextView label = root.findViewById(org.chromium.chrome.autofill_assistant.R.id.label);
        if (mLabel.isEmpty()) {
            label.setVisibility(View.GONE);
        } else {
            label.setText(mLabel);
        }

        LinearLayout container;
        if (mAllowMultipleChoices) {
            container = new LinearLayout(context);
            for (int i = 0; i < mChoices.size(); i++) {
                CheckBox checkBox = new CheckBox(context);
                initChoiceView(container, checkBox, i);
                checkBox.setChecked(mChoices.get(i).isInitiallySelected());
            }
        } else {
            // If only one choice can be selected, we need the parent to be a RadioGroup to make
            // sure there is always at most one selected choice.
            RadioGroup radioGroup = new RadioGroup(context);
            container = radioGroup;
            for (int i = 0; i < mChoices.size(); i++) {
                RadioButton radioButton = new RadioButton(context);
                initChoiceView(container, radioButton, i);

                if (mChoices.get(i).isInitiallySelected()) {
                    // We can't use the CompoundButton#setChecked method for radio buttons as this
                    // will not update the state of the parent RadioGroup, so we need to use the
                    // RadioGroup#check method instead.
                    radioGroup.check(radioButton.getId());
                }
            }
        }

        container.setOrientation(LinearLayout.VERTICAL);
        container.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        root.addView(container);
        return root;
    }

    private void initChoiceView(LinearLayout container, CompoundButton view, int index) {
        container.addView(view);
        AssistantFormSelectionChoice choice = mChoices.get(index);
        view.setText(choice.getLabel());
        view.setOnCheckedChangeListener(
                (unusedView, isChecked) -> mDelegate.onChoiceSelectionChanged(index, isChecked));
    }
}
