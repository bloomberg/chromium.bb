// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.graphics.Bitmap;

/**
 * A container to store the information for each Autofill Dialog menu item.
 */
public class AutofillDialogMenuItem {
    public final int mIndex;
    public final String mLine1;
    public final String mLine2;
    public final Bitmap mIcon;

    /**
     * @param index Index of the menu item.
     * @param line1 First line of the menu item.
     * @param line2 Second line of the menu item.
     * @param icon An icon for the menu item (might be null).
     */
    public AutofillDialogMenuItem(int index, String line1, String line2, Bitmap icon) {
        mIndex = index;
        mLine1 = line1;
        mLine2 = line2;
        mIcon = icon;
    }

    /**
     * @param index Index of the menu item.
     * @param label Label of the menu item.
     */
    public AutofillDialogMenuItem(int index, String label) {
        this(index, label, "", null);
    }
}
