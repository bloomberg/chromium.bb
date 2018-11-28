// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The different types of view that a suggestion can be. */
@IntDef({OmniboxSuggestionUiType.DEFAULT})
@Retention(RetentionPolicy.SOURCE)
@interface OmniboxSuggestionUiType {
    int DEFAULT = 0;
}
