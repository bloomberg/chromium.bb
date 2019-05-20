// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

import java.text.ChoiceFormat;

class AssistantFormCounter {
    private final String mLabel;
    private final String mSubtext;
    private final ChoiceFormat mLabelChoiceFormat;
    private final int mMinValue;
    private final int mMaxValue;
    private int mValue;

    AssistantFormCounter(
            String label, String subtext, int initialValue, int minValue, int maxValue) {
        mLabel = label;
        mSubtext = subtext;
        mLabelChoiceFormat = new ChoiceFormat(label);
        mValue = initialValue;
        mMinValue = minValue;
        mMaxValue = maxValue;
    }

    String getLabel() {
        return mLabel;
    }

    String getSubtext() {
        return mSubtext;
    }

    ChoiceFormat getLabelChoiceFormat() {
        return mLabelChoiceFormat;
    }

    int getMinValue() {
        return mMinValue;
    }

    int getMaxValue() {
        return mMaxValue;
    }

    int getValue() {
        return mValue;
    }

    public void setValue(int value) {
        mValue = value;
    }
}
