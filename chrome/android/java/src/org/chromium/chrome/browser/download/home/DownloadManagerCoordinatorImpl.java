// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.download.home.list.DateOrderedListCoordinator;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * The top level coordinator for the download home UI.  This is currently an in progress class and
 * is not fully fleshed out yet.
 */
public class DownloadManagerCoordinatorImpl {
    private final DateOrderedListCoordinator mListCoordinator;

    /** Builds a {@link DownloadManagerCoordinatorImpl} instance. */
    public DownloadManagerCoordinatorImpl(Profile profile, Context context) {
        mListCoordinator = new DateOrderedListCoordinator(
                context, OfflineContentAggregatorFactory.forProfile(profile));
    }

    /**
     * @return The {@link View} representing downloads home.
     */
    public View getView() {
        return mListCoordinator.getView();
    }
}