// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import java.util.ArrayList;
import java.util.Collection;

/**
 * The data to show in a single section where the user can select something, e.g., their
 * shipping address or payment method.
 */
public class SectionInformation {
    /**
     * This value indicates that the user has not made a selection in this section.
     */
    public static final int NO_SELECTION = -1;

    private final ArrayList<PaymentOption> mItems;
    private int mSelectedItem;

    /**
     * Builds an empty section without selection.
     */
    public SectionInformation() {
        mSelectedItem = NO_SELECTION;
        mItems = null;
    }

    /**
     * Builds a section with a single option, which is selected.
     *
     * @param defaultItem The only item. It is selected by default.
     */
    public SectionInformation(PaymentOption defaultItem) {
        if (null != defaultItem) {
            mSelectedItem = 0;
            mItems = new ArrayList<PaymentOption>(1);
            mItems.add(defaultItem);
        } else {
            mSelectedItem = NO_SELECTION;
            mItems = null;
        }
    }

    /**
     * Builds a section.
     *
     * @param selection The index of the currently selected item.
     * @param itemCollection The items in the section.
     */
    public SectionInformation(int selection, Collection<? extends PaymentOption> itemCollection) {
        mSelectedItem = selection;
        mItems = new ArrayList<PaymentOption>(itemCollection.size());
        mItems.addAll(itemCollection);
    }

    /**
     * Returns whether the section is empty.
     *
     * @return Whether the section is empty.
     */
    public boolean isEmpty() {
        return mItems == null || mItems.isEmpty();
    }

    /**
     * Returns the number of items in this section. For example, the number of shipping addresses or
     * payment methods.
     *
     * @return The number of items in this section.
     */
    public int getSize() {
        return mItems == null ? 0 : mItems.size();
    }

    /**
     * Returns the item in the given position.
     *
     * @param position The index of the item to return.
     * @return The item in the given position or null.
     */
    public PaymentOption getItem(int position) {
        if (mItems == null || mItems.isEmpty() || position < 0 || position >= mItems.size()) {
            return null;
        }

        return mItems.get(position);
    }

    /**
     * Sets the currently selected item by index.
     *
     * @param index The index of the currently selected item, or NO_SELECTION if a selection has not
     *              yet been made.
     */
    public void setSelectedItemIndex(int index) {
        mSelectedItem = index;
    }

    /**
     * Sets the currently selected item.
     *
     * @param selectedItem The currently selected item, or null of a selection has not yet been
     *                     made.
     */
    public void setSelectedItem(PaymentOption selectedItem) {
        if (mItems == null) return;
        for (int i = 0; i < mItems.size(); i++) {
            if (mItems.get(i) == selectedItem) {
                mSelectedItem = i;
                return;
            }
        }
    }

    /**
     * Returns the index of the selected item.
     *
     * @return The index of the currently selected item, or NO_SELECTION if a selection has not yet
     * been made.
     */
    public int getSelectedItemIndex() {
        return mSelectedItem;
    }

    /**
     * Returns the selected item, if any.
     *
     * @return The selected item or null if none selected.
     */
    public PaymentOption getSelectedItem() {
        return getItem(getSelectedItemIndex());
    }
}
