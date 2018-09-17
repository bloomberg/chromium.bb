// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ExploreSitesCategory} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExploreSitesCategoryUnitTest {
    @Test
    public void testAddSite() {
        final int id = 1;
        final int siteId = 100;
        final String title = "test";
        final String url = "http://www.google.com";
        final String categoryTitle = "Movies";

        ExploreSitesCategory category = new ExploreSitesCategory(id, categoryTitle);
        category.addSite(new ExploreSitesSite(siteId, title, url));

        assertEquals(id, category.getId());
        assertEquals(1, category.getSites().size());
        assertEquals(siteId, category.getSites().get(0).getId());
        assertEquals(title, category.getSites().get(0).getTitle());
        assertEquals(url, category.getSites().get(0).getUrl());
    }
}
