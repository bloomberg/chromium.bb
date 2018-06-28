// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.app.Activity;
import android.content.ComponentName;

import org.chromium.chrome.browser.download.ui.DownloadManagerUi;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager;

/** A helper class to build and return an {@link DownloadManagerCoordinator}. */
public class DownloadManagerCoordinatorFactory {
    // TODO(850603): Add feature flag to enable new downloads home.
    private static boolean sEnableDownloadsHomeV2 = false;

    /**
     * Returns an instance of a {@link DownloadManagerCoordinator} to be used in the UI.
     * @param activity           The parent {@link Activity}.
     * @param isOffTheRecord     Whether or not this UI should include off the record items.
     * @param parentComponent    The parent component.
     * @param isSeparateActivity Whether or not the UI is being shown as part of a separate
     *                           activity.
     * @param snackbarManager    The {@link SnackbarManager} that should be used to show snackbars.
     * @return                   A new {@link DownloadManagerCoordinator} instance.
     */
    public static DownloadManagerCoordinator create(Activity activity, boolean isOffTheRecord,
            SnackbarManager snackbarManager, ComponentName parentComponent,
            boolean isSeparateActivity) {
        if (sEnableDownloadsHomeV2) {
            return new DownloadManagerCoordinatorImpl(
                    Profile.getLastUsedProfile(), activity, isOffTheRecord, snackbarManager);
        } else {
            return new DownloadManagerUi(
                    activity, isOffTheRecord, parentComponent, isSeparateActivity, snackbarManager);
        }
    }
}