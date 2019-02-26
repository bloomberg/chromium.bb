// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.support.annotation.IntDef;

import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * State for the carousel of the Autofill Assistant.
 */
public class AssistantCarouselModel extends PropertyModel {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({Alignment.START, Alignment.CENTER, Alignment.END})
    public @interface Alignment {
        int START = 0;
        int CENTER = 1;
        int END = 2;
    }

    public static final WritableIntPropertyKey ALIGNMENT = new WritableIntPropertyKey();

    private final ListModel<AssistantChip> mChipsModel = new ListModel<>();

    public AssistantCarouselModel() {
        super(ALIGNMENT);
    }

    public ListModel<AssistantChip> getChipsModel() {
        return mChipsModel;
    }
}
