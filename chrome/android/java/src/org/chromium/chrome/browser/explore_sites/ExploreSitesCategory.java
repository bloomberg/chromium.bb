// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.UrlConstants;

import java.util.ArrayList;
import java.util.List;

/**
 * An object representing a category in Explore Sites.
 */
public class ExploreSitesCategory {
    // The ID to use when creating the More button, that should not scroll the ESP when clicked.
    public static final int MORE_BUTTON_ID = -1;

    private int mCategoryId;
    private String mCategoryTitle;

    // Populated only in NTP.
    private Drawable mDrawable;
    // Populated only for ESP.
    private List<ExploreSitesSite> mSites;

    /**
     * Creates an explore sites category data structure.
     * @param categoryId The integer category ID, corresponding to the enum value from the Catalog
     *   proto, or -1 if this represents the More button.
     * @param title The string to display as the caption for this tile.
     */
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

    public void setIcon(Context context, Bitmap icon) {
        mDrawable = new BitmapDrawable(context.getResources(), icon);
    }

    public void setDrawable(Drawable drawable) {
        mDrawable = drawable;
    }

    public Drawable getDrawable() {
        return mDrawable;
    }

    public void addSite(ExploreSitesSite site) {
        mSites.add(site);
    }

    public List<ExploreSitesSite> getSites() {
        return mSites;
    }

    /**
     * Returns the URL for the explore sites page, with the correct scrolling ID in the hash value.
     */
    public String getUrl() {
        if (mCategoryId < 0) {
            return UrlConstants.EXPLORE_URL;
        }
        return UrlConstants.EXPLORE_URL + "#" + getId();
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
