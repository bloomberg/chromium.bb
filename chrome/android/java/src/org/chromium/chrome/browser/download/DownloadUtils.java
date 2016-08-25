// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.download.ui.BackendProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.Toast;

/**
 * A class containing some utility static methods.
 */
public class DownloadUtils {

    private static final String EXTRA_IS_OFF_THE_RECORD =
            "org.chromium.chrome.browser.download.IS_OFF_THE_RECORD";

    /**
     * Displays the download manager UI. Note the UI is different on tablets and on phones.
     */
    public static void showDownloadManager(Activity activity, Tab tab) {
        if (DeviceFormFactor.isTablet(activity) && activity instanceof ChromeTabbedActivity) {
            // Download Home shows up inside a tab in the browser.
            LoadUrlParams params = new LoadUrlParams(UrlConstants.DOWNLOADS_URL);
            if (tab == null || !tab.isInitialized()) {
                TabDelegate delegate = new TabDelegate(false);
                delegate.createNewTab(params, TabLaunchType.FROM_CHROME_UI, null);
            } else {
                tab.loadUrl(params);
            }
        } else {
            // Download Home sits on top as a new Activity.
            Intent intent = new Intent();
            intent.setClass(activity, DownloadActivity.class);
            intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
            if (tab != null) intent.putExtra(EXTRA_IS_OFF_THE_RECORD, tab.isIncognito());
            activity.startActivity(intent);
        }
    }

    /**
     * @return Whether or not the Intent corresponds to a DownloadActivity that should show off the
     *         record downloads.
     */
    public static boolean shouldShowOffTheRecordDownloads(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_OFF_THE_RECORD, false);
    }

    /**
     * Records metrics related to downloading a page. Should be called after a tap on the download
     * page button.
     * @param tab The Tab containing the page being downloaded.
     */
    public static void recordDownloadPageMetrics(Tab tab) {
        RecordHistogram.recordPercentageHistogram("OfflinePages.SavePage.PercentLoaded",
                tab.getProgress());
    }

    /**
     * Shows a "Downloading..." toast. Should be called after a download has been started.
     * @param context The {@link Context} used to make the toast.
     */
    public static void showDownloadStartToast(Context context) {
        Toast.makeText(context, R.string.download_pending, Toast.LENGTH_SHORT).show();
    }

    /**
     * Issues a request to the {@link DownloadDelegate} associated with backendProvider to check
     * for externally removed downloads.
     * See {@link DownloadManagerService#checkForExternallyRemovedDownloads}.
     *
     * @param backendProvider The {@link BackendProvider} associated with the DownloadDelegate used
     *                        to check for externally removed downloads.
     * @param isOffTheRecord  Whether to check downloads for the off the record profile.
     */
    public static void checkForExternallyRemovedDownloads(BackendProvider backendProvider,
            boolean isOffTheRecord) {
        if (isOffTheRecord) {
            backendProvider.getDownloadDelegate().checkForExternallyRemovedDownloads(true);
        }
        backendProvider.getDownloadDelegate().checkForExternallyRemovedDownloads(false);
        RecordUserAction.record(
                "Android.DownloadManager.CheckForExternallyRemovedItems");
    }
}
