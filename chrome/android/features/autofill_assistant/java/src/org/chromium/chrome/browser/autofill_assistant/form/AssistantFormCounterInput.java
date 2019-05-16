// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.autofill_assistant.R;

import java.util.List;
import java.util.regex.Pattern;

/** A form input that allows to modify one or multiple counters. */
class AssistantFormCounterInput extends AssistantFormInput {
    private static final String QUOTED_VALUE = Pattern.quote("{value}");

    interface Delegate {
        void onCounterChanged(int counterIndex, int value);
    }

    private final String mLabel;
    private final List<AssistantFormCounter> mCounters;
    private final int mMinimizedCount;
    private final Delegate mDelegate;

    AssistantFormCounterInput(String label, List<AssistantFormCounter> counters, int minimizedCount,
            Delegate delegate) {
        mLabel = label;
        mCounters = counters;
        mMinimizedCount = minimizedCount;
        mDelegate = delegate;
    }

    @Override
    public View createView(Context context, ViewGroup parent) {
        ViewGroup root = (ViewGroup) LayoutInflater.from(context).inflate(
                R.layout.autofill_assistant_form_counter_input, parent, /* attachToRoot= */ false);
        TextView label = root.findViewById(R.id.label);
        if (mLabel.isEmpty()) {
            label.setVisibility(View.GONE);
        } else {
            label.setText(mLabel);
        }

        ViewGroup expandableSection = root.findViewById(R.id.expandable_section);
        int labelIndex = root.indexOfChild(label);
        for (int i = 0; i < mCounters.size(); i++) {
            View counterView = createCounterView(context, mCounters.get(i), i);
            if (i < mMinimizedCount) {
                // Add the counters below the label.
                root.addView(counterView, labelIndex + i + 1);
            } else {
                expandableSection.addView(counterView);
            }
        }

        // If some counters are in the expandable section, show the "Show more" label that will
        // expand it when clicked.
        if (expandableSection.getChildCount() > 0) {
            TextView expandLabel = root.findViewById(R.id.expand_label);
            expandLabel.setVisibility(View.VISIBLE);

            String expandString =
                    context.getResources().getString(R.string.autofill_assistant_form_show_more);
            String shrinkString =
                    context.getResources().getString(R.string.autofill_assistant_form_show_less);
            expandLabel.setText(expandString);
            expandLabel.setOnClickListener(unusedView -> {
                boolean expanded = expandableSection.getVisibility() == View.VISIBLE;
                if (expanded) {
                    // Shrink.
                    expandableSection.setVisibility(View.GONE);
                    expandLabel.setText(expandString);
                } else {
                    // Expand.
                    expandableSection.setVisibility(View.VISIBLE);
                    expandLabel.setText(shrinkString);
                }
            });
        }

        return root;
    }

    private View createCounterView(
            Context context, AssistantFormCounter counter, int counterIndex) {
        View view = LayoutInflater.from(context).inflate(
                R.layout.autofill_assistant_form_counter, /*root= */ null);
        TextView labelView = view.findViewById(R.id.label);
        updateLabel(counter, labelView);

        View decreaseButton = view.findViewById(R.id.decrease_button);
        View increaseButton = view.findViewById(R.id.increase_button);
        updateCounterState(counter, decreaseButton, increaseButton);

        decreaseButton.setOnClickListener(unusedView
                -> updateCounter(
                        counter, counterIndex, labelView, -1, decreaseButton, increaseButton));

        increaseButton.setOnClickListener(unusedView
                -> updateCounter(
                        counter, counterIndex, labelView, +1, decreaseButton, increaseButton));

        return view;
    }

    private void updateLabel(AssistantFormCounter counter, TextView labelView) {
        String label = counter.getLabel();
        if (counter.getLabelChoiceFormat().getLimits().length > 0) {
            label = counter.getLabelChoiceFormat().format(counter.getValue());
        }
        label = label.replaceAll(QUOTED_VALUE, Integer.toString(counter.getValue()));
        labelView.setText(label);
    }

    private void updateCounter(AssistantFormCounter counter, int counterIndex, TextView labelView,
            int delta, View decreaseButton, View increaseButton) {
        counter.setValue(counter.getValue() + delta);
        counter.setValue(Math.max(counter.getMinValue(), counter.getValue()));
        counter.setValue(Math.min(counter.getMaxValue(), counter.getValue()));
        updateCounterState(counter, decreaseButton, increaseButton);

        updateLabel(counter, labelView);
        mDelegate.onCounterChanged(counterIndex, counter.getValue());
    }

    private void updateCounterState(
            AssistantFormCounter counter, View decreaseButton, View increaseButton) {
        decreaseButton.setEnabled(counter.getValue() > counter.getMinValue());
        increaseButton.setEnabled(counter.getValue() < counter.getMaxValue());
    }
}
