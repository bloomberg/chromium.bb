// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.app.Activity;
import android.content.Context;

import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.download.settings.DownloadSettings;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** A helper class to build and return an {@link DownloadManagerCoordinator}. */
public class DownloadManagerCoordinatorFactoryHelper {
    /**
     * Returns an instance of a {@link DownloadManagerCoordinator} to be used in the UI.
     * @param activity           The parent {@link Activity}.
     * @param config             A {@link DownloadManagerUiConfig} to provide configuration params.
     * @param snackbarManager    The {@link SnackbarManager} that should be used to show snackbars.
     * @param modalDialogManager The {@link ModalDialogManager} that should be used to show dialog.
     * @return                   A new {@link DownloadManagerCoordinator} instance.
     */
    public static DownloadManagerCoordinator create(Activity activity,
            DownloadManagerUiConfig config, SnackbarManager snackbarManager,
            ModalDialogManager modalDialogManager) {
        Profile profile = config.isOffTheRecord
                ? Profile.getLastUsedRegularProfile().getOffTheRecordProfile()
                : Profile.getLastUsedRegularProfile();
        LegacyDownloadProvider legacyProvider =
                config.useNewDownloadPath ? null : new LegacyDownloadProviderImpl();
        return new DownloadManagerCoordinatorImpl(activity, config, new PrefetchEnabledSupplier(),
                DownloadManagerCoordinatorFactoryHelper::settingsLaunchHelper, snackbarManager,
                modalDialogManager, TrackerFactory.getTrackerForProfile(profile),
                new FaviconProviderImpl(profile), OfflineContentAggregatorFactory.get(),
                legacyProvider, GlobalDiscardableReferencePool.getReferencePool());
    }

    private static void settingsLaunchHelper(Context context) {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(context, DownloadSettings.class);
    }
}
