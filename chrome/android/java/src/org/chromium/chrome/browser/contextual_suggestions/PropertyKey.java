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
    @IntDef({ON_CLICK_LISTENER_PROPERTY})
    @Retention(RetentionPolicy.SOURCE)
    @interface Key {}
    static final int ON_CLICK_LISTENER_PROPERTY = 0;
    // TODO(twellington): Add a property for the toolbar title.

    @Key
    Integer mKey;

    PropertyKey(@Key Integer key) {
        mKey = key;
    }
}
