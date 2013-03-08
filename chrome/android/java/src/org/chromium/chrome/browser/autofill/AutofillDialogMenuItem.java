// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

/**
 * A container to store the information for each Autofill Dialog menu item.
 */
public class AutofillDialogMenuItem {
    public final int mUniqueId;
    public final String mLine1;
    public final String mLine2;

    /**
     * @param uniqueId ID of the menu item.
     * @param line1 First line of the menu item.
     * @param line2 Second line of the menu item.
     */
    public AutofillDialogMenuItem(int uniqueId, String line1, String line2) {
        mUniqueId = uniqueId;
        mLine1 = line1;
        mLine2 = line2;
    }
}
