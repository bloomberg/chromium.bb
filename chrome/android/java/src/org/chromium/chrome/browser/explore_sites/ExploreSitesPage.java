// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.app.Activity;
import android.content.Context;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.v7.widget.LinearLayoutManager;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.modelutil.ListModel;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.native_page.BasicNativePage;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Provides functionality when the user interacts with the explore sites page.
 */
public class ExploreSitesPage extends BasicNativePage {
    static final PropertyModel.WritableIntPropertyKey STATUS_KEY =
            new PropertyModel.WritableIntPropertyKey();
    static final PropertyModel.WritableIntPropertyKey SCROLL_TO_CATEGORY_KEY =
            new PropertyModel.WritableIntPropertyKey();
    static final PropertyModel
            .ReadableObjectPropertyKey<ListModel<ExploreSitesCategory>> CATEGORY_LIST_KEY =
            new PropertyModel.ReadableObjectPropertyKey<>();

    @IntDef({CatalogLoadingState.LOADING, CatalogLoadingState.SUCCESS, CatalogLoadingState.ERROR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CatalogLoadingState {
        int LOADING = 1;
        int SUCCESS = 2;
        int ERROR = 3;
    }

    private ViewGroup mView;
    private String mTitle;
    private Activity mActivity;
    private ExploreSitesPageLayout mLayout;
    private PropertyModel mModel;

    /**
     * Create a new instance of the explore sites page.
     */
    public ExploreSitesPage(ChromeActivity activity, NativePageHost host) {
        super(activity, host);
    }

    @Override
    protected void initialize(ChromeActivity activity, NativePageHost host) {
        mActivity = activity;
        mTitle = mActivity.getString(R.string.explore_sites_title);
        mView = (ViewGroup) mActivity.getLayoutInflater().inflate(
                R.layout.explore_sites_page_layout, null);

        mModel = new PropertyModel.Builder(STATUS_KEY, SCROLL_TO_CATEGORY_KEY, CATEGORY_LIST_KEY)
                         .with(CATEGORY_LIST_KEY, new ListModel<ExploreSitesCategory>())
                         .with(SCROLL_TO_CATEGORY_KEY, 0)
                         .build();
        Context context = mView.getContext();
        LinearLayoutManager layoutManager = new LinearLayoutManager(context);
        int iconSizePx = context.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_size);
        RoundedIconGenerator iconGenerator =
                new RoundedIconGenerator(iconSizePx, iconSizePx, iconSizePx / 2,
                        ApiCompatibilityUtils.getColor(
                                context.getResources(), R.color.default_favicon_background_color),
                        context.getResources().getDimensionPixelSize(R.dimen.headline_size_medium));

        ExploreSitesCategoryCardAdapter adapterDelegate =
                new ExploreSitesCategoryCardAdapter(mModel, layoutManager, iconGenerator);

        Profile profile = host.getActiveTab().getProfile();
        ExploreSitesBridge.getEspCatalog(profile, this::translateToModel);
        mModel.set(STATUS_KEY, CatalogLoadingState.LOADING);
        // TODO(chili): Set layout to be an observer of list model
    }

    private void translateToModel(@Nullable List<ExploreSitesCategory> categoryList) {
        ListModel<ExploreSitesCategory> categoryListModel = mModel.get(CATEGORY_LIST_KEY);
        categoryListModel.set(categoryList);
    }

    @Override
    public String getHost() {
        return UrlConstants.EXPLORE_HOST;
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }
}
