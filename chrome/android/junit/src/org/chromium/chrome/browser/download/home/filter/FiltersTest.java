// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.offline_items_collection.OfflineItemFilter;

/** Unit tests for the Filters class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FiltersTest {
    @Test
    public void testFilterConversions() {
        Assert.assertEquals(
                Filters.FilterType.SITES, Filters.fromOfflineItem(OfflineItemFilter.FILTER_PAGE));
        Assert.assertEquals(
                Filters.FilterType.VIDEOS, Filters.fromOfflineItem(OfflineItemFilter.FILTER_VIDEO));
        Assert.assertEquals(
                Filters.FilterType.MUSIC, Filters.fromOfflineItem(OfflineItemFilter.FILTER_AUDIO));
        Assert.assertEquals(
                Filters.FilterType.IMAGES, Filters.fromOfflineItem(OfflineItemFilter.FILTER_IMAGE));
        Assert.assertEquals(
                Filters.FilterType.OTHER, Filters.fromOfflineItem(OfflineItemFilter.FILTER_OTHER));
        Assert.assertEquals(Filters.FilterType.OTHER,
                Filters.fromOfflineItem(OfflineItemFilter.FILTER_DOCUMENT));
    }
}