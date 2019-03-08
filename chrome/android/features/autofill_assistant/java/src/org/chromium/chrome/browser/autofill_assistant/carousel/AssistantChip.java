// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A chip to display to the user.
 */
public class AssistantChip {
    @IntDef({Type.CHIP_ASSISTIVE, Type.BUTTON_FILLED_BLUE, Type.BUTTON_HAIRLINE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        int CHIP_ASSISTIVE = 0;
        int BUTTON_FILLED_BLUE = 1;
        int BUTTON_HAIRLINE = 2;

        /** The number of types. Increment this value if you add a type. */
        int CHIP_TYPE_NUMBER = 3;
    }

    private final @Type int mType;
    private final String mText;
    private final boolean mDisabled;
    private final Runnable mSelectedListener;

    public AssistantChip(@Type int type, String text, boolean disabled, Runnable selectedListener) {
        mType = type;
        mText = text;
        mSelectedListener = selectedListener;
        mDisabled = disabled;
    }

    public int getType() {
        return mType;
    }

    public String getText() {
        return mText;
    }

    public boolean isDisabled() {
        return mDisabled;
    }

    public Runnable getSelectedListener() {
        return mSelectedListener;
    }
}
