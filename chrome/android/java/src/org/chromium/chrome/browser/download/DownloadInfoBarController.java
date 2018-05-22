// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.os.Handler;
import android.support.annotation.Nullable;
import android.support.annotation.PluralsRes;
import android.text.TextUtils;
import android.text.format.Formatter;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.infobar.DownloadProgressInfoBar;
import org.chromium.chrome.browser.infobar.IPHInfoBarSupport;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.components.download.DownloadState;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * This class is responsible for tracking various updates for download items, offline items, android
 * downloads and computing the the state of the {@link DownloadProgressInfoBar} .
 */
public class DownloadInfoBarController implements OfflineContentProvider.Observer {
    public static final int INVALID_NOTIFICATION_ID = -1;
    private static final long DURATION_ACCELERATED_INFOBAR_IN_MS = 3000;
    private static final long DURATION_SHOW_RESULT_IN_MS = 6000;

    /**
     * Represents various UI states that the InfoBar cycles through.
     */
    private enum DownloadInfoBarState {
        // Default initial state. It is also the final state after all the downloads are paused or
        // removed. No InfoBar is shown in this state.
        INITIAL,

        // InfoBar is showing a message indicating the downloads in progress. In case of a single
        // accelerated download, the InfoBar would show the speeding-up download message for {@code
        // DURATION_ACCELERATED_INFOBAR_IN_MS} before transitioning to downloading file(s) message.
        // If download completes,fails or goes to pending state, the transition happens immediately
        // to SHOW_RESULT state.
        DOWNLOADING,

        // InfoBar is showing download complete, failed or pending message. The InfoBar stays in
        // this state for {@code DURATION_SHOW_RESULT_IN_MS} before transitioning to the next state,
        // which can be another SHOW_RESULT or DOWNLOADING state. This can also happen to be the
        // terminal state if there are no more updates to be shown.
        // In case of a new download, completed download or cancellation signal, the transition
        // happens immediately.
        SHOW_RESULT,

        // The state of the InfoBar after it was explicitly cancelled by the user. The InfoBar is
        // resurfaced only when there is a new download or an existing download moves to completion,
        // failed or pending state.
        CANCELLED,
    }

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

        // Keeps track of the current number of downloads in various states. Only used to compute
        // |forceReparent|.
        public DownloadCount downloadCount = new DownloadCount();

        // Used for differentiating various states (e.g. completed, failed, pending etc) in the
        // SHOW_RESULT state. Keeps track of the state of the currently displayed item(s) and should
        // be reset to null when moving out DOWNLOADING/SHOW_RESULT state.
        @OfflineItemState
        public Integer resultState;

        @Override
        public int hashCode() {
            int result = (id == null ? 0 : id.hashCode());
            result = 31 * result + (message == null ? 0 : message.hashCode());
            result = 31 * result + (link == null ? 0 : link.hashCode());
            result = 31 * result + icon;
            return result;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj == this) return true;
            if (!(obj instanceof DownloadProgressInfoBarData)) return false;

            DownloadProgressInfoBarData other = (DownloadProgressInfoBarData) obj;
            boolean idEquality = (id == null ? other.id == null : id.equals(other.id));
            return idEquality && TextUtils.equals(message, other.message)
                    && TextUtils.equals(link, other.link) && icon == other.icon;
        }
    }

    /**
     * An utility class to count the number of downloads at different states at any given time.
     */
    private static class DownloadCount {
        public int inProgress;
        public int pending;
        public int failed;
        public int completed;

        /** @return The total number of downloads being tracked. */
        public int totalCount() {
            return inProgress + pending + failed + completed;
        }

        public int getCount(@OfflineItemState int state) {
            switch (state) {
                case OfflineItemState.IN_PROGRESS:
                    return inProgress;
                case OfflineItemState.COMPLETE:
                    return completed;
                case OfflineItemState.FAILED:
                    return failed;
                case OfflineItemState.PENDING:
                    return pending;
                default:
                    assert false;
            }
            return 0;
        }

        @Override
        public int hashCode() {
            int result = 31 * inProgress;
            result = 31 * result + pending;
            result = 31 * result + failed;
            result = 31 * result + completed;
            return result;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj == this) return true;
            if (!(obj instanceof DownloadCount)) return false;

            DownloadCount other = (DownloadCount) obj;
            return inProgress == other.inProgress && pending == other.pending
                    && failed == other.failed && completed == other.completed;
        }
    }

    private final boolean mIsIncognito;
    private final Handler mHandler = new Handler();
    private final DownloadProgressInfoBar.Client mClient = new DownloadProgressInfoBarClient();

    // Keeps track of a running list of items, which gets updated regularly with every update from
    // the backend. The entries are removed only when the item has reached a certain completion
    // state (i.e. complete, failed or pending) or is cancelled/removed from the backend.
    private final LinkedHashMap<ContentId, OfflineItem> mTrackedItems = new LinkedHashMap<>();

    // Keeps track of all the items that have been seen in the current chrome session.
    private final Set<ContentId> mSeenItems = new HashSet<>();

    // Keeps track of the items which are being ignored by the controller, e.g. user initiated
    // paused items.
    private final Set<ContentId> mIgnoredItems = new HashSet<>();

    // The notification IDs associated with the currently tracked completed items. The notification
    // should be removed when the InfoBar link is clicked to open the item.
    private final Map<ContentId, Integer> mNotificationIds = new HashMap<>();

    // The current state of the InfoBar.
    private DownloadInfoBarState mState = DownloadInfoBarState.INITIAL;

    // This is used when the InfoBar is currently in a state awaiting timer completion, e.g. showing
    // the speeding-up message or showing the result of a download. This is used to schedule a task
    // to determine the next state. If the infobar moves out of the current state, the scheduled
    // task should be cancelled.
    private Runnable mEndTimerRunnable;

    // Represents the InfoBar, currently being handled by this controller.
    private DownloadProgressInfoBar mCurrentInfoBar;

    // Represents the currently displayed InfoBar data.
    private DownloadProgressInfoBarData mCurrentInfo;

    // The primary means of getting the currently active tab.
    private TabModelSelector mTabModelSelector;

    /** Constructor. */
    public DownloadInfoBarController(boolean isIncognito) {
        mIsIncognito = isIncognito;
        mHandler.post(() -> getOfflineContentProvider().addObserver(this));
    }

    /**
     * Sets the {@link TabModelSelector} that will be used to get the currently active tab.
     * @param selector A {@link TabModelSelector} that represents the state of the system.
     */
    public void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
    }

    /**
     * Shows the message that download has started. Unlike other methods in this class, this
     * method doesn't require an {@link OfflineItem} and is invoked by the backend to provide a
     * responsive feedback to the users even before the download has actually started.
     */
    public void onDownloadStarted() {
        computeNextStepForUpdate(null, true, false, false);
    }

    /** Updates the InfoBar when new information about a download comes in. */
    public void onDownloadItemUpdated(DownloadItem downloadItem) {
        OfflineItem offlineItem = DownloadInfo.createOfflineItem(downloadItem.getDownloadInfo());
        if (!isVisibleToUser(offlineItem)) return;

        if (downloadItem.getDownloadInfo().state() == DownloadState.COMPLETE) {
            handleDownloadCompletion(downloadItem);
            return;
        }

        if (offlineItem.state == OfflineItemState.CANCELLED) {
            onItemRemoved(offlineItem.id);
            return;
        }

        computeNextStepForUpdate(offlineItem);
    }

    /** Updates the InfoBar after a download has been removed. */
    public void onDownloadItemRemoved(ContentId contentId) {
        onItemRemoved(contentId);
    }

    /** Associates a notification ID with the tracked download for future usage. */
    public void onNotificationShown(ContentId id, int notificationId) {
        mNotificationIds.put(id, notificationId);
    }

    private void handleDownloadCompletion(DownloadItem downloadItem) {
        // Multiple OnDownloadUpdated() notifications may be issued while the
        // download is in the COMPLETE state. Don't handle it if it was previously not in-progress.
        if (!mTrackedItems.containsKey(downloadItem.getContentId())) return;

        // If the download should be auto-opened, we shouldn't show the infobar.
        DownloadManagerService.getDownloadManagerService().checkIfDownloadWillAutoOpen(
                downloadItem, (result) -> {
                    if (result) {
                        onItemRemoved(downloadItem.getContentId());
                    } else {
                        computeNextStepForUpdate(
                                DownloadInfo.createOfflineItem(downloadItem.getDownloadInfo()));
                    }
                });
    }

    // OfflineContentProvider.Observer implementation.
    @Override
    public void onItemsAdded(ArrayList<OfflineItem> items) {
        for (OfflineItem item : items) {
            if (!isVisibleToUser(item)) continue;
            computeNextStepForUpdate(item);
        }
    }

    @Override
    public void onItemRemoved(ContentId id) {
        if (!mTrackedItems.containsKey(id)) return;

        mTrackedItems.remove(id);
        mNotificationIds.remove(id);
        computeNextStepForUpdate(null, false, false, true);
    }

    @Override
    public void onItemUpdated(OfflineItem item) {
        if (!isVisibleToUser(item)) return;

        computeNextStepForUpdate(item);
    }

    /**
     * Helper method to get the parameters for showing accelerated download infobar IPH.
     * @return The UI parameters to show IPH, if an IPH should be shown, null otherwise.
     */
    public IPHInfoBarSupport.TrackerParameters getTrackerParameters() {
        if (getDownloadCount().inProgress == 0) return null;

        return new IPHInfoBarSupport.TrackerParameters(
                FeatureConstants.DOWNLOAD_INFOBAR_DOWNLOADS_ARE_FASTER_FEATURE,
                R.string.iph_download_infobar_downloads_are_faster_text,
                R.string.iph_download_infobar_downloads_are_faster_text);
    }

    private boolean isVisibleToUser(OfflineItem offlineItem) {
        if (offlineItem.isTransient) return false;
        if (offlineItem.isOffTheRecord != mIsIncognito) return false;
        if (offlineItem.isSuggested) return false;
        if (LegacyHelpers.isLegacyDownload(offlineItem.id)) {
            if (TextUtils.isEmpty(offlineItem.filePath)) {
                return false;
            }
        }

        return true;
    }

    private void computeNextStepForUpdate(OfflineItem updatedItem) {
        computeNextStepForUpdate(updatedItem, false, false, false);
    }

    /**
     * Updates the state of the infobar based on the update received and current state of the
     * tracked downloads.
     * @param updatedItem The item that was updated just now.
     * @param forceShowDownloadStarted Whether the infobar should show download started even if
     * there is no updated item.
     * @param userCancel Whether the infobar was cancelled just now.
     * ended.
     */
    private void computeNextStepForUpdate(OfflineItem updatedItem, boolean forceShowDownloadStarted,
            boolean userCancel, boolean itemWasRemoved) {
        if (updatedItem != null && mIgnoredItems.contains(updatedItem.id)) return;

        preProcessUpdatedItem(updatedItem);
        boolean isNewDownload = forceShowDownloadStarted
                || (updatedItem != null && updatedItem.state == OfflineItemState.IN_PROGRESS
                           && !mSeenItems.contains(updatedItem.id));

        if (updatedItem != null) {
            mTrackedItems.put(updatedItem.id, updatedItem);
            mSeenItems.add(updatedItem.id);
        }

        boolean itemWasPaused = updatedItem != null && updatedItem.state == OfflineItemState.PAUSED;
        if (itemWasPaused) {
            mIgnoredItems.add(updatedItem.id);
            mTrackedItems.remove(updatedItem.id);
        }

        DownloadCount downloadCount = getDownloadCount();
        boolean shouldShowResult =
                downloadCount.completed + downloadCount.failed + downloadCount.pending > 0;

        boolean shouldShowAccelerating =
                mEndTimerRunnable != null && mState == DownloadInfoBarState.DOWNLOADING;

        DownloadInfoBarState nextState = mState;

        switch (mState) {
            case INITIAL: // Intentional fallthrough.
            case CANCELLED:
                if (isNewDownload) {
                    nextState = DownloadInfoBarState.DOWNLOADING;
                    shouldShowAccelerating = updatedItem != null && updatedItem.isAccelerated
                            && downloadCount.inProgress == 1;
                } else if (shouldShowResult) {
                    nextState = DownloadInfoBarState.SHOW_RESULT;
                }

                break;

            case DOWNLOADING:
                if (isNewDownload) shouldShowAccelerating = false;

                if (shouldShowResult) {
                    nextState = DownloadInfoBarState.SHOW_RESULT;
                } else if (itemWasPaused || itemWasRemoved) {
                    nextState = downloadCount.inProgress == 0 ? DownloadInfoBarState.INITIAL
                                                              : DownloadInfoBarState.DOWNLOADING;
                }
                break;

            case SHOW_RESULT:
                if (isNewDownload) {
                    nextState = DownloadInfoBarState.DOWNLOADING;
                    shouldShowAccelerating = updatedItem != null && updatedItem.isAccelerated
                            && downloadCount.inProgress == 1;
                } else if (!shouldShowResult) {
                    if (mEndTimerRunnable == null && downloadCount.inProgress > 0) {
                        nextState = DownloadInfoBarState.DOWNLOADING;
                    }

                    boolean currentlyShowingPending = mCurrentInfo != null
                            && mCurrentInfo.resultState != null
                            && mCurrentInfo.resultState == OfflineItemState.PENDING;
                    if (currentlyShowingPending && itemResumedFromPending(updatedItem)) {
                        nextState = DownloadInfoBarState.DOWNLOADING;
                    }
                    if (itemWasRemoved && mTrackedItems.size() == 0) {
                        nextState = DownloadInfoBarState.INITIAL;
                    }
                }

                break;
        }

        if (userCancel) nextState = DownloadInfoBarState.CANCELLED;

        moveToState(nextState, shouldShowAccelerating);
    }

    private void moveToState(DownloadInfoBarState nextState, boolean showAccelerating) {
        boolean closeInfoBar = nextState == DownloadInfoBarState.INITIAL
                || nextState == DownloadInfoBarState.CANCELLED;
        if (closeInfoBar) {
            mCurrentInfo = null;
            closePreviousInfoBar();
            if (nextState == DownloadInfoBarState.INITIAL) {
                mTrackedItems.clear();
            } else {
                clearFinishedItems(OfflineItemState.COMPLETE, OfflineItemState.FAILED,
                        OfflineItemState.PENDING);
            }
            clearEndTimerRunnable();
        }

        if (nextState == DownloadInfoBarState.DOWNLOADING
                || nextState == DownloadInfoBarState.SHOW_RESULT) {
            Integer offlineItemState = findOfflineItemStateForInfoBarState(nextState);
            if (offlineItemState == null) {
                // This is expected in the terminal SHOW_RESULT state when we have cleared the
                // tracked items but still want to show the infobar indefinitely.
                return;
            }
            createInfoBarForState(nextState, offlineItemState, showAccelerating);
        }

        mState = nextState;
    }

    /**
     * Determines the {@link OfflineItemState} for the message to be shown on the infobar. For
     * DOWNLOADING state, it will return {@link OfflineItemState.IN_PROGRESS}. Otherwise it should
     * show the result state which can be complete, failed or pending. There is usually a delay of
     * DURATION_SHOW_RESULT_IN_MS between transition between these states, except for the complete
     * state which must be shown as soon as received. While the InfoBar is in one of these states,
     * if we get another download update for the same state, we incorporate that in the existing
     * message and reset the timer to another full duration. Updates for pending and failed would be
     * shown in the order received.
     */
    private Integer findOfflineItemStateForInfoBarState(DownloadInfoBarState infoBarState) {
        if (infoBarState == DownloadInfoBarState.DOWNLOADING) return OfflineItemState.IN_PROGRESS;

        assert infoBarState == DownloadInfoBarState.SHOW_RESULT;

        DownloadCount downloadCount = getDownloadCount();

        // If there are completed downloads, show immediately.
        if (downloadCount.completed > 0) return OfflineItemState.COMPLETE;

        // If the infobar is already showing this state, just add this item to the same state.
        Integer previousResultState = mCurrentInfo != null ? mCurrentInfo.resultState : null;
        if (previousResultState != null && downloadCount.getCount(previousResultState) > 0) {
            return previousResultState;
        }

        // Show any failed or pending states in the order they were received.
        for (OfflineItem item : mTrackedItems.values()) {
            if (item.state == OfflineItemState.FAILED || item.state == OfflineItemState.PENDING) {
                return item.state;
            }
        }

        return null;
    }

    /**
     * Prepares the infobar to show the next state. This can be one of the messages i.e. speeding-up
     * download, downloading files or showing result e.g. complete, failed, pending message.
     * @param infoBarState The infobar state to be shown.
     * @param offlineItemState The state of the corresponding offline items to be shown.
     */
    private void createInfoBarForState(DownloadInfoBarState infoBarState,
            @OfflineItemState Integer offlineItemState, boolean showAccelerating) {
        DownloadProgressInfoBarData info = new DownloadProgressInfoBarData();

        @PluralsRes
        int stringRes = -1;
        if (infoBarState == DownloadInfoBarState.DOWNLOADING) {
            info.icon = showAccelerating ? R.drawable.infobar_downloading_sweep_animation
                                         : R.drawable.infobar_downloading_fill_animation;
        } else if (offlineItemState == OfflineItemState.COMPLETE) {
            stringRes = R.plurals.multiple_download_complete;
            info.icon = R.drawable.infobar_download_complete_animation;
        } else if (offlineItemState == OfflineItemState.FAILED) {
            stringRes = R.plurals.multiple_download_failed;
            info.icon = R.drawable.ic_error_outline_googblue_24dp;
        } else if (offlineItemState == OfflineItemState.PENDING) {
            stringRes = R.plurals.multiple_download_pending;
            info.icon = R.drawable.ic_error_outline_googblue_24dp;
        } else {
            assert false : "Unexptected offlineItemState " + offlineItemState + " and infoBarState "
                           + infoBarState;
        }

        OfflineItem itemToShow = null;
        long totalDownloadingSizeBytes = 0L;
        for (OfflineItem item : mTrackedItems.values()) {
            if (item.state != offlineItemState) continue;
            itemToShow = item;
            totalDownloadingSizeBytes += item.totalSizeBytes;
        }

        DownloadCount downloadCount = getDownloadCount();
        if (infoBarState == DownloadInfoBarState.DOWNLOADING) {
            if (showAccelerating) {
                info.message =
                        getContext().getString(R.string.download_infobar_speeding_up_download);
            } else {
                int inProgressDownloadCount =
                        downloadCount.inProgress == 0 ? 1 : downloadCount.inProgress;
                if (totalDownloadingSizeBytes <= 0L) {
                    info.message = getContext().getResources().getQuantityString(
                            R.plurals.download_infobar_downloading_files, inProgressDownloadCount,
                            inProgressDownloadCount);
                } else {
                    String bytesString =
                            Formatter.formatFileSize(getContext(), totalDownloadingSizeBytes);
                    info.message = inProgressDownloadCount == 1
                            ? getContext().getString(
                                      R.string.downloading_file_with_bytes, bytesString)
                            : getContext().getString(R.string.downloading_multiple_files_with_bytes,
                                      inProgressDownloadCount, bytesString);
                }
            }

            info.hasAnimation = true;
            info.link = showAccelerating ? null : getContext().getString(R.string.details_link);
        } else if (infoBarState == DownloadInfoBarState.SHOW_RESULT) {
            int itemCount = getDownloadCount().getCount(offlineItemState);
            boolean singleDownloadCompleted =
                    itemCount == 1 && offlineItemState == OfflineItemState.COMPLETE;
            if (singleDownloadCompleted) {
                info.message = itemToShow.title;
                info.id = itemToShow.id;
                info.link = getContext().getString(R.string.open_downloaded_label);
                info.hasAnimation = true;
                info.dontRepeat = true;
            } else {
                // TODO(shaktisahu): Incorporate various types of failure messages.
                info.message = getContext().getResources().getQuantityString(
                        stringRes, itemCount, itemCount);
                info.link = getContext().getString(R.string.details_link);
            }
        }

        info.resultState = isResultState(offlineItemState) ? offlineItemState : null;

        if (info.equals(mCurrentInfo)) return;

        boolean startTimer = showAccelerating || infoBarState == DownloadInfoBarState.SHOW_RESULT;

        clearEndTimerRunnable();

        if (startTimer) {
            long delay = showAccelerating ? DURATION_ACCELERATED_INFOBAR_IN_MS
                                          : DURATION_SHOW_RESULT_IN_MS;
            mEndTimerRunnable = () -> {
                mEndTimerRunnable = null;
                if (mCurrentInfo != null) mCurrentInfo.resultState = null;
                if (infoBarState == DownloadInfoBarState.SHOW_RESULT) {
                    clearFinishedItems(offlineItemState);
                }
                computeNextStepForUpdate(null, false, false, false);
            };
            mHandler.postDelayed(mEndTimerRunnable, delay);
        }

        setForceReparent(info);
        showInfoBar(info);
    }

    private void setForceReparent(DownloadProgressInfoBarData info) {
        info.downloadCount = getDownloadCount();
        info.forceReparent = !info.downloadCount.equals(
                mCurrentInfo == null ? null : mCurrentInfo.downloadCount);
    }

    private void clearEndTimerRunnable() {
        mHandler.removeCallbacks(mEndTimerRunnable);
        mEndTimerRunnable = null;
    }

    private boolean isResultState(@OfflineItemState int offlineItemState) {
        return offlineItemState == OfflineItemState.COMPLETE
                || offlineItemState == OfflineItemState.FAILED
                || offlineItemState == OfflineItemState.PENDING;
    }

    private void preProcessUpdatedItem(OfflineItem updatedItem) {
        if (updatedItem == null) return;

        // INTERRUPTED downloads should be treated as PENDING in the infobar. From here onwards,
        // there should be no INTERRUPTED state in the core logic.
        if (updatedItem.state == OfflineItemState.INTERRUPTED) {
            updatedItem.state = OfflineItemState.PENDING;
        }
    }

    private boolean itemResumedFromPending(OfflineItem updatedItem) {
        if (updatedItem == null || !mTrackedItems.containsKey(updatedItem.id)) return false;

        return mTrackedItems.get(updatedItem.id).state == OfflineItemState.PENDING
                && updatedItem.state == OfflineItemState.IN_PROGRESS;
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

    private void maybeShowDownloadsStillInProgressIPH() {
        if (getDownloadCount().inProgress == 0) return;
        if (getCurrentTab() == null
                || !(getCurrentTab().getActivity() instanceof ChromeTabbedActivity)) {
            return;
        }

        ChromeTabbedActivity activity = (ChromeTabbedActivity) getCurrentTab().getActivity();
        Profile profile = mIsIncognito ? Profile.getLastUsedProfile().getOffTheRecordProfile()
                                       : Profile.getLastUsedProfile().getOriginalProfile();
        final Tracker tracker = TrackerFactory.getTrackerForProfile(profile);

        tracker.addOnInitializedCallback((Callback<Boolean>) success
                -> activity.maybeShowAppMenuTextBubble(tracker,
                        FeatureConstants.DOWNLOAD_INFOBAR_DOWNLOAD_CONTINUING_FEATURE,
                        R.id.downloads_menu_id,
                        R.string.iph_download_infobar_download_continuing_text,
                        R.string.iph_download_infobar_download_continuing_text));
    }

    @Nullable
    private Tab getCurrentTab() {
        if (mTabModelSelector == null) return null;
        return TabModelUtils.getCurrentTab(mTabModelSelector.getModel(mIsIncognito));
    }

    private Context getContext() {
        return ContextUtils.getApplicationContext();
    }

    private DownloadCount getDownloadCount() {
        DownloadCount downloadCount = new DownloadCount();
        for (OfflineItem item : mTrackedItems.values()) {
            switch (item.state) {
                case OfflineItemState.IN_PROGRESS:
                    downloadCount.inProgress++;
                    break;
                case OfflineItemState.COMPLETE:
                    downloadCount.completed++;
                    break;
                case OfflineItemState.FAILED:
                    downloadCount.failed++;
                    break;
                case OfflineItemState.CANCELLED:
                    break;
                case OfflineItemState.PENDING:
                    downloadCount.pending++;
                    break;
                case OfflineItemState.INTERRUPTED: // intentional fall through
                case OfflineItemState.PAUSED: // intentional fall through
                default:
                    assert false;
            }
        }

        return downloadCount;
    }

    /**
     * Clears the items in finished state, i.e. completed, failed or pending.
     * @param states States to be removed.
     */
    private void clearFinishedItems(Integer... states) {
        Set<Integer> statesToRemove = new HashSet<>(Arrays.asList(states));
        List<ContentId> idsToRemove = new ArrayList<>();
        for (ContentId id : mTrackedItems.keySet()) {
            OfflineItem item = mTrackedItems.get(id);
            if (item == null) continue;
            if (statesToRemove.contains(item.state)) {
                idsToRemove.add(id);
            }
        }

        for (ContentId id : idsToRemove) {
            mTrackedItems.remove(id);
            mNotificationIds.remove(id);
        }
    }

    private OfflineContentProvider getOfflineContentProvider() {
        return OfflineContentAggregatorFactory.forProfile(
                Profile.getLastUsedProfile().getOriginalProfile());
    }

    private class DownloadProgressInfoBarClient implements DownloadProgressInfoBar.Client {
        @Override
        public void onLinkClicked(ContentId itemId) {
            mTrackedItems.remove(itemId);
            mNotificationIds.remove(itemId);
            if (itemId != null) {
                DownloadUtils.openItem(
                        itemId, mIsIncognito, DownloadMetrics.DOWNLOAD_PROGRESS_INFO_BAR);
            } else {
                DownloadManagerService.getDownloadManagerService().openDownloadsPage(getContext());
            }
            closePreviousInfoBar();
        }

        @Override
        public void onInfoBarClosed(boolean explicitly) {
            // TODO(shaktisahu) : Remove param |explicitly| if it ends up unused.
            if (explicitly) {
                maybeShowDownloadsStillInProgressIPH();
                computeNextStepForUpdate(null, false, true, false);
            }
        }
    }
}
