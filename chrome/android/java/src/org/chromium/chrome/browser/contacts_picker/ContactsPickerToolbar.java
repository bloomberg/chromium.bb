// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.Context;
import android.support.v4.widget.ImageViewCompat;
import android.support.v7.widget.AppCompatImageView;
import android.util.AttributeSet;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.selection.SelectableListToolbar;
import org.chromium.ui.widget.ButtonCompat;

import java.util.List;

/**
 * Handles toolbar functionality for the {@ContactsPickerDialog}.
 */
public class ContactsPickerToolbar extends SelectableListToolbar<ContactDetails> {
    // Our parent dialog.
    ContactsPickerDialog mDialog;

    public ContactsPickerToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Set the parent dialog for this toolbar.
     */
    public void setParentDialog(ContactsPickerDialog dialog) {
        mDialog = dialog;
    }

    /**
     * Shows the Back arrow navigation button in the upper left corner.
     */
    public void showBackArrow() {
        setNavigationButton(NAVIGATION_BUTTON_BACK);
    }

    @Override
    public void onNavigationBack() {
        if (isSearching()) {
            super.onNavigationBack();
        } else {
            mDialog.cancel();
        }
    }

    @Override
    public void onSelectionStateChange(List<ContactDetails> selectedItems) {
        super.onSelectionStateChange(selectedItems);

        int selectCount = selectedItems.size();
        ButtonCompat done = findViewById(R.id.done);
        done.setEnabled(selectCount > 0);

        AppCompatImageView search = findViewById(R.id.search);
        ImageViewCompat.setImageTintList(search,
                useDarkIcons() ? getDarkIconColorStateList() : getLightIconColorStateList());

        if (selectCount > 0) {
            ApiCompatibilityUtils.setTextAppearance(done, R.style.TextAppearance_Body_Inverse);
        } else {
            ApiCompatibilityUtils.setTextAppearance(
                    done, R.style.TextAppearance_BlackDisabledText3);

            showBackArrow();
        }
    }
}
