// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.graphics.Bitmap;

import org.chromium.base.annotations.CalledByNative;

import java.util.ArrayList;
import java.util.List;

/**
 * An object representing a category in Explore Sites.
 */
public class ExploreSitesCategory {
    private int mCategoryId;

    private String mCategoryTitle;
    // Populated only in NTP.
    private Bitmap mIcon;
    // Populated only for ESP.
    private List<ExploreSitesSite> mSites;

    public ExploreSitesCategory(int categoryId, String title) {
        mCategoryId = categoryId;
        mCategoryTitle = title;
        mSites = new ArrayList<>();
    }

    public int getId() {
        return mCategoryId;
    }

    public String getTitle() {
        return mCategoryTitle;
    }

    public void setIcon(Bitmap icon) {
        mIcon = icon;
    }

    public Bitmap getIcon() {
        return mIcon;
    }

    public void addSite(ExploreSitesSite site) {
        mSites.add(site);
    }

    public List<ExploreSitesSite> getSites() {
        return mSites;
    }

    // Creates a new category and appends to the given list. Also returns the created category to
    // easily append sites to the category.
    @CalledByNative
    private static ExploreSitesCategory createAndAppendToList(
            int categoryId, String title, List<ExploreSitesCategory> list) {
        ExploreSitesCategory category = new ExploreSitesCategory(categoryId, title);
        list.add(category);
        return category;
    }
}
