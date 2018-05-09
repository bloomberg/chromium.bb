// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.support.annotation.Nullable;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.infobar.DownloadProgressInfoBar;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.components.offline_items_collection.ContentId;

/**
 * This class is responsible for tracking various updates for download items, offline items, android
 * downloads and computing the the state of the {@link DownloadProgressInfoBar} .
 */
public class DownloadInfoBarController {
    /**
     * Represents the data required to show UI elements of the {@link DownloadProgressInfoBar}..
     */
    public static class DownloadProgressInfoBarData {
        @Nullable
        public ContentId id;

        public String message;
        public String link;
        public int icon;

        // Whether the icon should have animation.
        public boolean hasAnimation;

        // Whether the animation should be shown only once.
        public boolean dontRepeat;

        // Whether the the InfoBar must be shown in the current tab, even if the user has navigated
        // to a different tab. In that case, the InfoBar associated with the previous tab is closed
        // and a new InfoBar is created with the currently focused tab.
        public boolean forceReparent;
    }

    private final boolean mIsIncognito;
    private DownloadProgressInfoBar.Client mClient = new DownloadProgressInfoBarClient();

    // The InfoBar, currently being handled by this controller.
    private DownloadProgressInfoBar mCurrentInfoBar;

    // Represents the currently displayed InfoBar data.
    private DownloadProgressInfoBarData mCurrentInfo;

    /** Constructor. */
    public DownloadInfoBarController(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }

    /**
     * Central function called to show an InfoBar. If the previous InfoBar was on a different
     * tab which is currently not active, based on the value of |info.forceReparent|, it is
     * determines whether to close the InfoBar and recreate or simply update the existing InfoBar.
     * @param info Contains the information to be displayed in the UI.
     */
    private void showInfoBar(DownloadProgressInfoBarData info) {
        if (!FeatureUtilities.isDownloadProgressInfoBarEnabled()) return;

        mCurrentInfo = info;

        Tab currentTab = getCurrentTab();
        if (currentTab != null && (info.forceReparent || mCurrentInfoBar == null)) {
            showInfoBarForCurrentTab(info);
        } else {
            updateExistingInfoBar(info);
        }
    }

    private void showInfoBarForCurrentTab(DownloadProgressInfoBarData info) {
        Tab currentTab = getCurrentTab();
        Tab prevTab = mCurrentInfoBar != null ? mCurrentInfoBar.getTab() : null;

        if (currentTab != prevTab) closePreviousInfoBar();
        if (mCurrentInfoBar == null) {
            createInfoBar(info);
        } else {
            updateExistingInfoBar(info);
        }
    }

    private void createInfoBar(DownloadProgressInfoBarData infoBarData) {
        if (!FeatureUtilities.isDownloadProgressInfoBarEnabled()) return;

        Tab currentTab = getCurrentTab();
        if (currentTab == null) return;

        currentTab.getInfoBarContainer().addObserver(mInfoBarContainerObserver);
        DownloadProgressInfoBar.createInfoBar(mClient, currentTab, infoBarData);
    }

    private void updateExistingInfoBar(DownloadProgressInfoBarData info) {
        if (mCurrentInfoBar == null) return;

        mCurrentInfoBar.updateInfoBar(info);
    }

    private void closePreviousInfoBar() {
        if (mCurrentInfoBar == null) return;

        Tab prevTab = mCurrentInfoBar.getTab();
        if (prevTab != null) {
            prevTab.getInfoBarContainer().removeObserver(mInfoBarContainerObserver);
        }

        mCurrentInfoBar.closeInfoBar();
        mCurrentInfoBar = null;
    }

    private InfoBarContainer.InfoBarContainerObserver mInfoBarContainerObserver =
            new InfoBarContainer.InfoBarContainerObserver() {
                @Override
                public void onAddInfoBar(
                        InfoBarContainer container, InfoBar infoBar, boolean isFirst) {
                    if (infoBar.getInfoBarIdentifier()
                            != InfoBarIdentifier.DOWNLOAD_PROGRESS_INFOBAR_ANDROID) {
                        return;
                    }

                    mCurrentInfoBar = (DownloadProgressInfoBar) infoBar;
                }

                @Override
                public void onRemoveInfoBar(
                        InfoBarContainer container, InfoBar infoBar, boolean isLast) {
                    if (infoBar.getInfoBarIdentifier()
                            != InfoBarIdentifier.DOWNLOAD_PROGRESS_INFOBAR_ANDROID) {
                        return;
                    }

                    mCurrentInfoBar = null;
                    container.removeObserver(this);
                }

                @Override
                public void onInfoBarContainerAttachedToWindow(boolean hasInfobars) {}

                @Override
                public void onInfoBarContainerShownRatioChanged(
                        InfoBarContainer container, float shownRatio) {}
            };

    @Nullable
    private Tab getCurrentTab() {
        // TODO(shaktisahu): Use a TabModelSelector instead.
        if (!ApplicationStatus.hasVisibleActivities()) return null;
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof ChromeActivity)) return null;
        return ((ChromeActivity) activity).getActivityTab();
    }

    private class DownloadProgressInfoBarClient implements DownloadProgressInfoBar.Client {
        @Override
        public void onLinkClicked(ContentId itemId) {
            // TODO(shaktisahu): Add logic.
        }

        @Override
        public void onInfoBarClosed(boolean explicitly) {
            // TODO(shaktisahu): Add logic.
        }
    }
}
