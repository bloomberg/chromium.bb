// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link ExploreSitesSection}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExploreSitesSectionUnitTest {
    private static final int ID = 1;
    private static final int TYPE = 2;
    private static final String TITLE = "foo";

    @Test
    public void categoriesWithInteractionCount() {
        ExploreSitesCategory cat1 = new ExploreSitesCategory(ID, TYPE, TITLE, 0, 5);
        ExploreSitesCategory cat2 = new ExploreSitesCategory(ID, TYPE, TITLE, 0, 4);
        ExploreSitesCategory cat3 = new ExploreSitesCategory(ID, TYPE, TITLE, 0, 3);
        ExploreSitesCategory cat4 = new ExploreSitesCategory(ID, TYPE, TITLE, 3, 0);

        // Interaction is sorted in descending order.
        // 5 > 4, so 5 is first.
        assertTrue(ExploreSitesSection.compareCategoryPriority(cat1, cat2) < 0);
        // 3 < 4, so 4 is first.
        assertTrue(ExploreSitesSection.compareCategoryPriority(cat3, cat2) > 0);
        // 5 == 5.
        assertEquals(0, ExploreSitesSection.compareCategoryPriority(cat1, cat1));
        // 0s are not treated special.
        assertTrue(ExploreSitesSection.compareCategoryPriority(cat3, cat4) < 0);
    }

    @Test
    public void categoriesWithShownCount() {
        ExploreSitesCategory cat1 = new ExploreSitesCategory(ID, TYPE, TITLE, 2, 0);
        ExploreSitesCategory cat2 = new ExploreSitesCategory(ID, TYPE, TITLE, 4, 0);
        ExploreSitesCategory cat3 = new ExploreSitesCategory(ID, TYPE, TITLE, 5, 0);
        ExploreSitesCategory cat4 = new ExploreSitesCategory(ID, TYPE, TITLE, 3, 0);

        // Mods are sorted first in descending order.
        // 4 % 3 < 5 % 3, so 5 is first.
        assertTrue(ExploreSitesSection.compareCategoryPriority(cat2, cat3) > 0);
        // 2 % 3 > 4 % 3, so 2 is first.
        assertTrue(ExploreSitesSection.compareCategoryPriority(cat1, cat2) < 0);
        // If mods are equal, sort by integer division in ascending order.
        // 2 / 3 < 5 / 3, so 2 is first
        assertTrue(ExploreSitesSection.compareCategoryPriority(cat1, cat3) < 0);
        // If everything is equal, return equal.
        assertEquals(0, ExploreSitesSection.compareCategoryPriority(cat1, cat1));
    }
}