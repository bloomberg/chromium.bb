// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import org.chromium.chrome.R;

class CondensedCategoryCardViewHolderFactory extends CategoryCardViewHolderFactory {
    public CondensedCategoryCardViewHolderFactory() {
        super();
    }

    @Override
    protected int getCategoryCardViewResource() {
        return R.layout.explore_sites_category_card_view_condensed;
    }

    @Override
    protected int getTileViewResource() {
        return R.layout.explore_sites_tile_view_condensed;
    }
}
