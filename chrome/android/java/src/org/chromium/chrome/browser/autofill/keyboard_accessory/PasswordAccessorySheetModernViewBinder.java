// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.v7.widget.RecyclerView;
import android.text.method.PasswordTransformationMethod;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetTabModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetTabViewBinder.ElementViewHolder;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.widget.ChipView;

/**
 * This stateless class provides methods to bind a {@link ListModel<AccessorySheetDataPiece>}
 * to the {@link RecyclerView} used as view of a tab for the accessory sheet component.
 */
class PasswordAccessorySheetModernViewBinder {
    static ElementViewHolder create(ViewGroup parent, @AccessorySheetDataPiece.Type int viewType) {
        switch (viewType) {
            case AccessorySheetDataPiece.Type.PASSWORD_INFO:
                return new PasswordInfoViewHolder(parent);
            case AccessorySheetDataPiece.Type.TITLE:
                return new AccessorySheetTabViewBinder.TitleViewHolder(
                        parent, R.layout.keyboard_accessory_sheet_tab_title);
            case AccessorySheetDataPiece.Type.FOOTER_COMMAND:
                return AccessorySheetTabViewBinder.create(parent, viewType);
        }
        assert false : "Unhandled type of data piece: " + viewType;
        return null;
    }

    /**
     * Holds a TextView that represents a list entry.
     */
    static class PasswordInfoViewHolder
            extends ElementViewHolder<KeyboardAccessoryData.UserInfo, PasswordAccessoryInfoView> {
        PasswordInfoViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_sheet_tab_password_info);
        }

        @Override
        protected void bind(KeyboardAccessoryData.UserInfo info, PasswordAccessoryInfoView view) {
            bindChipView(view.getUsername(), info.getFields().get(0));
            bindChipView(view.getPassword(), info.getFields().get(1));

            view.setIconForBitmap(null); // Set the default icon, then try to get a better one.
            if (info.getFaviconProvider() != null) {
                info.getFaviconProvider().fetchFavicon(
                        itemView.getContext().getResources().getDimensionPixelSize(
                                R.dimen.keyboard_accessory_suggestion_icon_size),
                        view::setIconForBitmap);
            }
        }

        void bindChipView(ChipView chip, KeyboardAccessoryData.UserInfo.Field field) {
            chip.getPrimaryTextView().setTransformationMethod(
                    field.isObfuscated() ? new PasswordTransformationMethod() : null);
            chip.getPrimaryTextView().setText(field.getDisplayText());
            chip.getPrimaryTextView().setContentDescription(field.getA11yDescription());
            chip.setOnClickListener(!field.isSelectable() ? null : src -> field.triggerSelection());
            chip.setClickable(field.isSelectable());
            chip.setEnabled(field.isSelectable());
        }
    }

    static void initializeView(RecyclerView view, AccessorySheetTabModel model) {
        view.setAdapter(PasswordAccessorySheetCoordinator.createModernAdapter(model));
        view.addItemDecoration(new PasswordAccessoryInfoView.DynamicBottomSpacer());
    }
}