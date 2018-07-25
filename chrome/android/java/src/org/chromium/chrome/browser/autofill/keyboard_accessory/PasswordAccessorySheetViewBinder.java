// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.support.annotation.Nullable;
import android.support.v4.view.MarginLayoutParamsCompat;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.text.method.PasswordTransformationMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.modelutil.SimpleListObservable;

/**
 * This stateless class provides methods to bind the items in a {@link SimpleListObservable<Item>}
 * to the {@link RecyclerView} used as view of the Password accessory sheet component.
 */
class PasswordAccessorySheetViewBinder {
    /**
     * Holds any View that represents a list entry.
     */
    static class ItemViewHolder extends RecyclerView.ViewHolder {
        ItemViewHolder(View itemView) {
            super(itemView);
        }

        static ItemViewHolder create(ViewGroup parent, @ItemType int viewType) {
            switch (viewType) {
                case ItemType.LABEL:
                    return new TextViewHolder(
                            LayoutInflater.from(parent.getContext())
                                    .inflate(R.layout.password_accessory_sheet_label, parent,
                                            false));
                case ItemType.SUGGESTION: // Intentional fallthrough.
                case ItemType.NON_INTERACTIVE_SUGGESTION: {
                    return new IconTextViewHolder(
                            LayoutInflater.from(parent.getContext())
                                    .inflate(R.layout.password_accessory_sheet_suggestion, parent,
                                            false));
                }
                case ItemType.DIVIDER:
                    return new ItemViewHolder(
                            LayoutInflater.from(parent.getContext())
                                    .inflate(R.layout.password_accessory_sheet_divider, parent,
                                            false));
                case ItemType.OPTION:
                    return new TextViewHolder(
                            LayoutInflater.from(parent.getContext())
                                    .inflate(R.layout.password_accessory_sheet_option, parent,
                                            false));
            }
            assert false : viewType;
            return null;
        }

        /**
         * Binds the item's state to the held {@link View}. Subclasses of this generic view holder
         * might want to actually bind the item state to the view.
         * @param item The item that determines the state of the held View.
         */
        protected void bind(Item item) {}
    }

    /**
     * Holds a TextView that represents a list entry.
     */
    static class TextViewHolder extends ItemViewHolder {
        TextViewHolder(View itemView) {
            super(itemView);
        }

        /**
         * Returns the text view of this item if there is one.
         * @return Returns a {@link TextView}.
         */
        protected TextView getTextView() {
            return (TextView) itemView;
        }

        @Override
        protected void bind(Item item) {
            super.bind(item);
            getTextView().setTransformationMethod(
                    item.isPassword() ? new PasswordTransformationMethod() : null);
            getTextView().setText(item.getCaption());
            if (item.getItemSelectedCallback() != null) {
                getTextView().setOnClickListener(
                        src -> item.getItemSelectedCallback().onResult(item));
            } else {
                getTextView().setOnClickListener(null);
            }
        }
    }

    /**
     * Holds a TextView that represents a list entry.
     */
    static class IconTextViewHolder extends TextViewHolder {
        private final TextView mSuggestionText;
        private final int mMargin;
        private final int mIconSize;

        IconTextViewHolder(View itemView) {
            super(itemView);
            mSuggestionText = itemView.findViewById(R.id.suggestion_text);
            mMargin = itemView.getContext().getResources().getDimensionPixelSize(
                    R.dimen.keyboard_accessory_suggestion_margin);
            mIconSize = itemView.getContext().getResources().getDimensionPixelSize(
                    R.dimen.keyboard_accessory_suggestion_icon_size);
        }

        @Override
        protected TextView getTextView() {
            return mSuggestionText;
        }

        @Override
        protected void bind(Item item) {
            super.bind(item);
            ViewGroup.MarginLayoutParams params =
                    new ViewGroup.MarginLayoutParams(mSuggestionText.getLayoutParams());
            MarginLayoutParamsCompat.setMarginEnd(params, mMargin);
            if (!item.isPassword()) {
                setIconForBitmap(null); // Set the default icon, then try to get a better one.
                item.fetchFavicon(this::setIconForBitmap);
                MarginLayoutParamsCompat.setMarginStart(params, mMargin);
            } else {
                ApiCompatibilityUtils.setCompoundDrawablesRelative(
                        mSuggestionText, null, null, null, null);
                MarginLayoutParamsCompat.setMarginStart(params, 2 * mMargin + mIconSize);
            }
            mSuggestionText.setLayoutParams(params);
        }

        private void setIconForBitmap(@Nullable Bitmap favicon) {
            Drawable icon;
            if (favicon == null) {
                icon = AppCompatResources.getDrawable(
                        itemView.getContext(), R.drawable.ic_globe_36dp);
            } else {
                icon = new BitmapDrawable(itemView.getContext().getResources(), favicon);
            }
            if (icon != null) { // AppCompatResources.getDrawable is @Nullable.
                icon.setBounds(0, 0, mIconSize, mIconSize);
            }
            mSuggestionText.setCompoundDrawablePadding(mMargin);
            ApiCompatibilityUtils.setCompoundDrawablesRelative(
                    mSuggestionText, icon, null, null, null);
        }
    }

    static void initializeView(RecyclerView view, RecyclerViewAdapter adapter) {
        view.setLayoutManager(
                new LinearLayoutManager(view.getContext(), LinearLayoutManager.VERTICAL, false));
        view.setItemAnimator(null);
        view.setAdapter(adapter);
    }
}
