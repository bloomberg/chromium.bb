// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.support.annotation.IntDef;

/**
 * A chip to display to the user.
 */
public class AssistantChip {
    /**
     * The type a chip can have. The terminology is borrow from:
     *  - https://guidelines.googleplex.com/googlematerial/components/chips.html
     *  - https://guidelines.googleplex.com/googlematerial/components/buttons.html
     */
    @IntDef({TYPE_CHIP_ASSISTIVE, TYPE_BUTTON_FILLED_BLUE, TYPE_BUTTON_TEXT})
    public @interface Type {}
    public static final int TYPE_CHIP_ASSISTIVE = 0;
    public static final int TYPE_BUTTON_FILLED_BLUE = 1;
    public static final int TYPE_BUTTON_TEXT = 2;

    private final @Type int mType;
    private final String mText;
    private final Runnable mSelectedListener;

    public AssistantChip(@Type int type, String text, Runnable selectedListener) {
        mType = type;
        mText = text;
        mSelectedListener = selectedListener;
    }

    public int getType() {
        return mType;
    }

    public String getText() {
        return mText;
    }

    public Runnable getSelectedListener() {
        return mSelectedListener;
    }
}
