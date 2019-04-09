// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.details;

/**
 * Holds information controlling whether to show image clickthrough dialog and
 * how to customize the dialog.
 */
/*package*/ class ImageClickthroughData {
    private final boolean mAllowClickthrough;
    private final String mDescription;
    private final String mPositiveText;
    private final String mNegativeText;

    ImageClickthroughData(boolean allowClickthrough, String description, String positiveText,
            String negativeText) {
        mAllowClickthrough = allowClickthrough;
        mDescription = description;
        mPositiveText = positiveText;
        mNegativeText = negativeText;
    }

    boolean getAllowClickthrough() {
        return mAllowClickthrough;
    }

    /**
     * The description text in the clickthrough dialog.
     */
    String getDescription() {
        return mDescription;
    }

    /**
     * The text appear on positive button of clickthrough dialog.
     */
    String getPositiveText() {
        return mPositiveText;
    }

    /**
     * The text appear on negative button of clickthrough dialog.
     */
    String getNegativeText() {
        return mNegativeText;
    }
}
