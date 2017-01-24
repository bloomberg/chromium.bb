// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Payloads to be passed to mark the different types of partial binds that can be made on view
 * holders
 * @see org.chromium.chrome.browser.ntp.cards.InnerNode#notifyItemChanged(int, Object)
 */
@IntDef({PartialUpdateId.OFFLINE_BADGE})
@Retention(RetentionPolicy.SOURCE)
public @interface PartialUpdateId {
    /** Marks a request to update a suggestion's offline badge */
    int OFFLINE_BADGE = 1;
}
