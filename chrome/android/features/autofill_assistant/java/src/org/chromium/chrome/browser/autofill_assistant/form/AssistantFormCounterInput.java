// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

import android.content.Context;
import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.Transition;
import android.transition.TransitionManager;
import android.transition.TransitionSet;
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
    private static final Transition EXPAND_TRANSITION =
            new TransitionSet()
                    .setOrdering(TransitionSet.ORDERING_TOGETHER)
                    .addTransition(new Fade(Fade.OUT))
                    .addTransition(new ChangeBounds())
                    .addTransition(new Fade(Fade.IN));

    interface Delegate {
        void onCounterChanged(int counterIndex, int value);
    }

    private final String mLabel;
    private final String mExpandText;
    private final String mMinimizeText;
    private final List<AssistantFormCounter> mCounters;
    private final int mMinimizedCount;
    private final Delegate mDelegate;

    AssistantFormCounterInput(String label, String expandText, String minimizeText,
            List<AssistantFormCounter> counters, int minimizedCount, Delegate delegate) {
        mLabel = label;
        mExpandText = expandText;
        mMinimizeText = minimizeText;
        mCounters = counters;

        // Don't show the expandable section if there is no text to show when minimized/expanded.
        mMinimizedCount =
                expandText.isEmpty() || minimizeText.isEmpty() ? Integer.MAX_VALUE : minimizedCount;
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

        // If some counters are in the expandable section, show the expand label that will expand
        // the section when clicked.
        if (expandableSection.getChildCount() > 0) {
            View expandLabelContainer = root.findViewById(R.id.expand_label_container);
            TextView expandLabel = root.findViewById(R.id.expand_label);
            TextView minimizeLabel = root.findViewById(R.id.minimize_label);
            View chevron = root.findViewById(R.id.chevron);

            expandLabel.setText(mExpandText);
            minimizeLabel.setText(mMinimizeText);
            minimizeLabel.setVisibility(View.GONE);
            expandLabelContainer.setVisibility(View.VISIBLE);

            expandLabelContainer.setOnClickListener(unusedView -> {
                expandLabel.setOnClickListener(null);
                expandableSection.setVisibility(View.VISIBLE);
                expandLabelContainer.setVisibility(View.GONE);
                expandLabel.setVisibility(View.GONE);
            });

            expandLabelContainer.setOnClickListener(unusedView -> {
                TransitionManager.beginDelayedTransition(
                        (ViewGroup) expandableSection.getRootView(), EXPAND_TRANSITION);
                boolean expanded = expandableSection.getVisibility() == View.VISIBLE;
                if (expanded) {
                    // Shrink.
                    expandableSection.setVisibility(View.GONE);
                    expandLabel.setVisibility(View.VISIBLE);
                    minimizeLabel.setVisibility(View.GONE);
                    chevron.animate().rotation(0).start();
                } else {
                    // Expand.
                    expandableSection.setVisibility(View.VISIBLE);
                    expandLabel.setVisibility(View.GONE);
                    minimizeLabel.setVisibility(View.VISIBLE);
                    chevron.animate().rotation(180).start();
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

        TextView subtextView = view.findViewById(R.id.subtext);
        if (!counter.getSubtext().isEmpty()) {
            subtextView.setVisibility(View.VISIBLE);
            subtextView.setText(counter.getSubtext());
        }

        TextView valueTextView = view.findViewById(R.id.value);
        View decreaseButton = view.findViewById(R.id.decrease_button);
        View increaseButton = view.findViewById(R.id.increase_button);
        updateCounterState(counter, valueTextView, decreaseButton, increaseButton);

        decreaseButton.setOnClickListener(unusedView
                -> updateCounter(counter, counterIndex, labelView, -1, valueTextView,
                        decreaseButton, increaseButton));

        increaseButton.setOnClickListener(unusedView
                -> updateCounter(counter, counterIndex, labelView, +1, valueTextView,
                        decreaseButton, increaseButton));

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
            int delta, TextView valueTextView, View decreaseButton, View increaseButton) {
        counter.setValue(counter.getValue() + delta);
        counter.setValue(Math.max(counter.getMinValue(), counter.getValue()));
        counter.setValue(Math.min(counter.getMaxValue(), counter.getValue()));
        updateCounterState(counter, valueTextView, decreaseButton, increaseButton);

        updateLabel(counter, labelView);
        mDelegate.onCounterChanged(counterIndex, counter.getValue());
    }

    // It is ok to suppress the warning here as we are only setting a number value to the TextView.
    @SuppressWarnings("SetTextI18n")
    private void updateCounterState(AssistantFormCounter counter, TextView valueTextView,
            View decreaseButton, View increaseButton) {
        valueTextView.setText(Integer.toString(counter.getValue()));
        decreaseButton.setEnabled(counter.getValue() > counter.getMinValue());
        increaseButton.setEnabled(counter.getValue() < counter.getMaxValue());
    }
}
