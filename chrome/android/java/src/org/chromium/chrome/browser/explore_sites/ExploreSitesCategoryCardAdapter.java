// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.ListObservableImpl;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyObservable;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Recycler view adapter delegate for a model containing a list of category cards and an error code.
 */
public class ExploreSitesCategoryCardAdapter extends ListObservableImpl<ExploreSitesCategory>
        implements RecyclerViewAdapter.Delegate<
                           ExploreSitesCategoryCardViewHolderFactory.CategoryCardViewHolder,
                           ExploreSitesCategory>,
                   PropertyObservable.PropertyObserver<PropertyKey> {
    @IntDef({ViewType.HEADER, ViewType.CATEGORY, ViewType.LOADING, ViewType.ERROR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ViewType {
        int HEADER = 0;
        int CATEGORY = 1;
        int LOADING = 2;
        int ERROR = 3;
    }

    private RecyclerView.LayoutManager mLayoutManager;
    private PropertyModel mCategoryModel;
    private RoundedIconGenerator mIconGenerator;

    public ExploreSitesCategoryCardAdapter(PropertyModel model,
            RecyclerView.LayoutManager layoutManager, RoundedIconGenerator iconGenerator) {
        model.addObserver(this);
        mCategoryModel = model;
        mIconGenerator = iconGenerator;
    }

    @Override
    public int getItemCount() {
        // If not loaded, return 2 for title and error/loading section
        if (mCategoryModel.get(ExploreSitesPage.STATUS_KEY)
                != ExploreSitesPage.CatalogLoadingState.SUCCESS) {
            return 2;
        }
        // Add 1 for title.
        return mCategoryModel.get(ExploreSitesPage.CATEGORY_LIST_KEY).size() + 1;
    }

    @Override
    @ViewType
    public int getItemViewType(int position) {
        if (position == 0) return ViewType.HEADER;
        if (mCategoryModel.get(ExploreSitesPage.STATUS_KEY)
                == ExploreSitesPage.CatalogLoadingState.ERROR) {
            return ViewType.ERROR;
        }
        if (mCategoryModel.get(ExploreSitesPage.STATUS_KEY)
                == ExploreSitesPage.CatalogLoadingState.LOADING) {
            return ViewType.LOADING;
        }
        return ViewType.CATEGORY;
    }

    @Override
    public void onBindViewHolder(
            ExploreSitesCategoryCardViewHolderFactory.CategoryCardViewHolder holder, int position,
            @Nullable ExploreSitesCategory payload) {
        if (holder.getItemViewType() == ViewType.HEADER) {
            TextView view = (TextView) holder.itemView;
            view.setText(R.string.explore_sites_title);
        } else if (holder.getItemViewType() == ViewType.ERROR) {
            // populate the error view
        } else if (holder.getItemViewType() == ViewType.LOADING) {
            // Populate loading view
        } else {
            ExploreSitesCategoryCardView view = (ExploreSitesCategoryCardView) holder.itemView;
            view.setIconGenerator(mIconGenerator);
            // Position - 1 because there is always title.
            view.initialize(
                    mCategoryModel.get(ExploreSitesPage.CATEGORY_LIST_KEY).get(position - 1));
        }
    }

    @Override
    public void onPropertyChanged(
            PropertyObservable<PropertyKey> source, @Nullable PropertyKey key) {
        if (key == ExploreSitesPage.STATUS_KEY) {
            int status = mCategoryModel.get(ExploreSitesPage.STATUS_KEY);
            if (status == ExploreSitesPage.CatalogLoadingState.LOADING
                    || status == ExploreSitesPage.CatalogLoadingState.ERROR) {
                notifyItemChanged(1);
            } // Else the list observer takes care of updating.
        }
        if (key == ExploreSitesPage.SCROLL_TO_CATEGORY_KEY) {
            int pos = mCategoryModel.get(ExploreSitesPage.SCROLL_TO_CATEGORY_KEY);
            mLayoutManager.scrollToPosition(pos + 1);
        }
    }
}
