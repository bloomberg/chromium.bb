// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Contains a key uniquely identifying properties held in the {@link ContextualSuggestionsModel}.
 */
class PropertyKey {
    /** The unique identifiers for properties held in the model. */
    @IntDef({CLOSE_BUTTON_ON_CLICK_LISTENER, TITLE})
    @Retention(RetentionPolicy.SOURCE)
    @interface Key {}
    static final int CLOSE_BUTTON_ON_CLICK_LISTENER = 0;
    static final int TITLE = 1;

    @Key
    Integer mKey;

    PropertyKey(@Key Integer key) {
        mKey = key;
    }
}
