// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.download.home.filter.chips.ChipsCoordinator;

/**
 * An empty version of {@code FilterCoordinator} that doesn't contain any tabs.
 */

public class FilterCoordinatorWithNoTabs extends FilterCoordinator {
    private final ChipsCoordinator mChipsCoordinator;
    private final FilterChipsProvider mChipsProvider;

    public FilterCoordinatorWithNoTabs(Context context, OfflineItemFilterSource chipFilterSource) {
        mChipsProvider = new FilterChipsProvider(type -> handleChipSelected(), chipFilterSource);
        mChipsCoordinator = new ChipsCoordinator(context, mChipsProvider);
    }

    @Override
    public View getView() {
        return mChipsCoordinator.getView();
    }

    @Override
    public void setSelectedFilter(int filter) {
        mChipsProvider.setFilterSelected(filter);
    }

    private void handleChipSelected() {
        notifyFilterChanged(mChipsProvider.getSelectedFilter());
    }
}
