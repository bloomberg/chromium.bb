// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.content.Context;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi.DownloadUiObserver;
import org.chromium.chrome.browser.widget.selection.SelectionToolbar;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Handles toolbar functionality for the {@link DownloadManagerUi}.
 */
public class DownloadManagerToolbar extends SelectionToolbar<DownloadHistoryItemWrapper>
        implements DownloadUiObserver {
    public DownloadManagerToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        inflateMenu(R.menu.download_manager_menu);
    }

    @Override
    public void initialize(DownloadManagerUi manager) {
        if (DeviceFormFactor.isTablet(getContext())) {
            getMenu().findItem(R.id.close_menu_id).setVisible(false);
        }
    }

    @Override
    public void onFilterChanged(int filter) {
        if (filter == DownloadFilter.FILTER_ALL) {
            setTitle(R.string.menu_downloads);
        } else {
            setTitle(DownloadFilter.getStringIdForFilter(filter));
        }
    }

    @Override
    public void onManagerDestroyed(DownloadManagerUi manager) {
    }
}
