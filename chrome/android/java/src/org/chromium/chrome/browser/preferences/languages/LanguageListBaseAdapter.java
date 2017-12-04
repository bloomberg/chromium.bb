// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.languages;

import android.support.annotation.DrawableRes;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.ListMenuButton;
import org.chromium.chrome.browser.widget.TintedImageView;

import java.util.ArrayList;
import java.util.List;

/**
 * BaseAdapter for {@link RecyclerView}. It manages languages to list there.
 */
public class LanguageListBaseAdapter
        extends RecyclerView.Adapter<LanguageListBaseAdapter.LanguageRowViewHolder> {
    /**
     * Listener used to respond to click event on a langauge item.
     */
    interface ItemClickListener {
        /**
         * @param item The clicked LanguageItem.
         */
        void onLanguageClicked(LanguageItem item);
    }

    static class LanguageRowViewHolder extends RecyclerView.ViewHolder {
        private View mRow;
        private TextView mTitle;
        private TextView mDescription;

        private TintedImageView mStartIcon;
        private ListMenuButton mMoreButton;

        private ItemClickListener mItemClickListener;

        LanguageRowViewHolder(View view) {
            super(view);
            mRow = view;
            mTitle = (TextView) view.findViewById(R.id.title);
            mDescription = (TextView) view.findViewById(R.id.description);

            mStartIcon = (TintedImageView) view.findViewById(R.id.icon_view);
            mMoreButton = (ListMenuButton) view.findViewById(R.id.more);
        }

        private void setDisplayName(LanguageItem item) {
            mTitle.setText(item.getDisplayName());
            // Avoid duplicate display names.
            if (TextUtils.equals(item.getDisplayName(), item.getNativeDisplayName())) {
                mDescription.setVisibility(View.GONE);
            } else {
                mDescription.setVisibility(View.VISIBLE);
                mDescription.setText(item.getNativeDisplayName());
            }
        }

        /**
         * Binds this {@link LanguageRowViewHolder} with the given params.
         * @param item The {@link LanguageItem} with language details.
         * @param itemClickListener Callback for the click event on this row.
         * @param menuButtonDelegate A {@link ListMenuButton.Delegate} to set up an optional menu
         *                           button at the end of this row.
         * @param startIconId An optional icon at the beginning of this row.
         */
        void bindView(LanguageItem item, ItemClickListener itemClickListener,
                ListMenuButton.Delegate menuButtonDelegate, @DrawableRes int startIconId) {
            setDisplayName(item);

            if (startIconId != 0) {
                mStartIcon.setImageResource(startIconId);
            } else {
                mStartIcon.setVisibility(View.GONE);
            }

            if (menuButtonDelegate != null) {
                mMoreButton.setDelegate(menuButtonDelegate);
            } else {
                mMoreButton.setVisibility(View.GONE);
            }

            if (itemClickListener != null) {
                mRow.setOnClickListener(view -> itemClickListener.onLanguageClicked(item));
            }
        }
    }

    private List<LanguageItem> mLanguageList;

    LanguageListBaseAdapter() {
        mLanguageList = new ArrayList<>();
    }

    @Override
    public LanguageRowViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View row = LayoutInflater.from(parent.getContext())
                           .inflate(R.layout.accept_languages_item, parent, false);
        return new LanguageRowViewHolder(row);
    }

    @Override
    public void onBindViewHolder(LanguageRowViewHolder holder, int position) {
        holder.bindView(mLanguageList.get(position), null, null, 0);
    }

    @Override
    public int getItemCount() {
        return mLanguageList.size();
    }

    LanguageItem getItemByPosition(int position) {
        return mLanguageList.get(position);
    }

    void reload(List<LanguageItem> languageList) {
        mLanguageList.clear();
        mLanguageList.addAll(languageList);
        notifyDataSetChanged();
    }
}
