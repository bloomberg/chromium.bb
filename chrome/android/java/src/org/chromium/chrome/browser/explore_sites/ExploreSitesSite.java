// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.graphics.Bitmap;

import org.chromium.base.annotations.CalledByNative;

/**
 * An object encapsulating info for a website.
 */
public class ExploreSitesSite {
    private int mSiteId;
    private String mSiteTitle;
    private Bitmap mIcon;
    private String mUrl;

    public ExploreSitesSite(int id, String title, String url) {
        mSiteId = id;
        mSiteTitle = title;
        mUrl = url;
    }

    public int getId() {
        return mSiteId;
    }

    public void setIcon(Bitmap icon) {
        mIcon = icon;
    }

    public String getTitle() {
        return mSiteTitle;
    }

    public String getUrl() {
        return mUrl;
    }

    public Bitmap getIcon() {
        return mIcon;
    }

    @CalledByNative
    private static void createSiteInCategory(
            int siteId, String title, String url, ExploreSitesCategory category) {
        ExploreSitesSite site = new ExploreSitesSite(siteId, title, url);
        category.addSite(site);
    }
}
