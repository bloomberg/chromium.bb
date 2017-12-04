// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.languages;

import android.content.Context;
import android.preference.Preference;
import android.support.v7.widget.DividerItemDecoration;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.ListMenuButton;
import org.chromium.chrome.browser.widget.ListMenuButton.Item;
import org.chromium.chrome.browser.widget.TintedDrawable;

/**
 * A preference that displays the current accept language list.
 */
public class LanguageListPreference extends Preference {
    // TODO(crbug/783049): Make the item in the list drag-able.
    private static class LanguageListAdapter
            extends LanguageListBaseAdapter implements LanguagesManager.AcceptLanguageObserver {
        private Context mContext;

        LanguageListAdapter(Context context) {
            super();
            mContext = context;
        }

        @Override
        public void onBindViewHolder(
                LanguageListBaseAdapter.LanguageRowViewHolder holder, int position) {
            final LanguageItem info = getItemByPosition(position);
            holder.bindView(info, null, new ListMenuButton.Delegate() {
                @Override
                public Item[] getItems() {
                    return new Item[] {
                            new Item(mContext, R.string.languages_item_option_offer_to_translate,
                                    R.drawable.ic_check_googblue_24dp, info.isSupported()),
                            new Item(mContext, R.string.remove, getItemCount() > 1)};
                }

                @Override
                public void onItemSelected(Item item) {
                    if (item.getTextId() == R.string.languages_item_option_offer_to_translate) {
                        // TODO(crbug/783049): Handle "offer to translate" event.
                    } else if (item.getTextId() == R.string.remove) {
                        LanguagesManager.getInstance().removeFromAcceptLanguages(info.getCode());
                    }
                }
            }, R.drawable.ic_drag_handle_grey600_24dp);
        }

        @Override
        public void onDataUpdated() {
            reload(LanguagesManager.getInstance().getUserAcceptLanguageItems());
        }
    }

    private View mView;
    private Button mAddLanguageButton;
    private RecyclerView mRecyclerView;
    private LanguageListAdapter mAdapter;

    private AddLanguageFragment.Launcher mLauncher;

    public LanguageListPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAdapter = new LanguageListAdapter(context);
    }

    @Override
    public View onCreateView(ViewGroup parent) {
        assert mLauncher != null;

        if (mView != null) return mView;

        mView = super.onCreateView(parent);

        mAddLanguageButton = (Button) mView.findViewById(R.id.add_language);
        ApiCompatibilityUtils.setCompoundDrawablesRelativeWithIntrinsicBounds(mAddLanguageButton,
                TintedDrawable.constructTintedDrawable(
                        getContext().getResources(), R.drawable.plus, R.color.pref_accent_color),
                null, null, null);
        mAddLanguageButton.setOnClickListener(view -> mLauncher.launchAddLanguage());

        mRecyclerView = (RecyclerView) mView.findViewById(R.id.language_list);
        LinearLayoutManager layoutMangager = new LinearLayoutManager(getContext());
        mRecyclerView.setLayoutManager(layoutMangager);
        mRecyclerView.addItemDecoration(
                new DividerItemDecoration(getContext(), layoutMangager.getOrientation()));

        return mView;
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);

        // We do not want the RecyclerView to be announced by screen readers every time
        // the view is bound.
        if (mRecyclerView.getAdapter() != mAdapter) {
            mRecyclerView.setAdapter(mAdapter);
            LanguagesManager.getInstance().setAcceptLanguageObserver(mAdapter);
            // Initialize accept language list.
            mAdapter.reload(LanguagesManager.getInstance().getUserAcceptLanguageItems());
        }
    }

    /**
     * Register a launcher for AddLanguageFragment. Preference's host fragment should call
     * this in its onCreate().
     */
    void registerActivityLauncher(AddLanguageFragment.Launcher launcher) {
        mLauncher = launcher;
    }
}
