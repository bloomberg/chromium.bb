// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.ui.widget.ChipView;

/**
 * The {@link ViewHolder} responsible for reflecting an {@link AssistantChip} to a {@link
 * TextView}.
 */
class AssistantChipViewHolder extends ViewHolder {
    private final View mView;
    private final TextView mText;

    private AssistantChipViewHolder(View view, TextView itemView) {
        super(view);
        mView = view;
        mText = itemView;
    }

    static AssistantChipViewHolder create(ViewGroup parent, int viewType) {
        LayoutInflater layoutInflater = LayoutInflater.from(parent.getContext());
        View view = null;
        TextView textView = null;
        switch (viewType % AssistantChip.Type.NUM_ENTRIES) {
            // TODO: inflate normal chrome buttons instead.
            case AssistantChip.Type.CHIP_ASSISTIVE:
                ChipView chipView = new ChipView(parent.getContext(), R.style.AssistiveChip);
                chipView.setLayoutParams(new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
                view = chipView;
                textView = chipView.getPrimaryTextView();
                break;
            case AssistantChip.Type.BUTTON_FILLED_BLUE:
                view = layoutInflater.inflate(
                        R.layout.autofill_assistant_button_filled, /* root= */ null);
                textView = (TextView) view;
                break;
            case AssistantChip.Type.BUTTON_HAIRLINE:
                view = layoutInflater.inflate(
                        R.layout.autofill_assistant_button_hairline, /* root= */ null);
                textView = (TextView) view;
                break;
            default:
                assert false : "Unsupported view type " + viewType;
        }

        if (viewType >= AssistantChip.Type.NUM_ENTRIES) {
            view.setEnabled(false);
        }

        return new AssistantChipViewHolder(view, textView);
    }

    static int getViewType(AssistantChip chip) {
        // We add AssistantChip.Type.CHIP_TYPE_NUMBER to differentiate between enabled and disabled
        // chips of the same type. Ideally, we should return a (type, disabled) tuple but
        // RecyclerView does not allow that.
        if (chip.isDisabled()) {
            return chip.getType() + AssistantChip.Type.NUM_ENTRIES;
        }

        return chip.getType();
    }

    public void bind(AssistantChip chip) {
        mText.setText(chip.getText());
        mView.setOnClickListener(ignoredView -> chip.getSelectedListener().run());
    }
}
