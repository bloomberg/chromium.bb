// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.app.Activity;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageNotificationBridge;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

/**
 * {@link SuggestionsUiDelegate} implementation.
 */
public class SuggestionsNavigationDelegateImpl implements SuggestionsNavigationDelegate {
    private static final String CHROME_CONTENT_SUGGESTIONS_REFERRER =
            "https://www.googleapis.com/auth/chrome-content-suggestions";

    private final Activity mActivity;
    private final Profile mProfile;

    private final Tab mTab;
    private final TabModelSelector mTabModelSelector;

    public SuggestionsNavigationDelegateImpl(
            Activity activity, Profile profile, Tab currentTab, TabModelSelector tabModelSelector) {
        mActivity = activity;
        mProfile = profile;
        mTab = currentTab;
        mTabModelSelector = tabModelSelector;
    }

    @Override
    public boolean isOpenInNewWindowEnabled() {
        return MultiWindowUtils.getInstance().isOpenInOtherWindowSupported(mActivity);
    }

    @Override
    public boolean isOpenInIncognitoEnabled() {
        return PrefServiceBridge.getInstance().isIncognitoModeEnabled();
    }

    @Override
    public void navigateToBookmarks() {
        RecordUserAction.record("MobileNTPSwitchToBookmarks");
        BookmarkUtils.showBookmarkManager(mActivity);
    }

    @Override
    public void navigateToRecentTabs() {
        RecordUserAction.record("MobileNTPSwitchToOpenTabs");
        mTab.loadUrl(new LoadUrlParams(UrlConstants.RECENT_TABS_URL));
    }

    @Override
    public void navigateToDownloadManager() {
        RecordUserAction.record("MobileNTPSwitchToDownloadManager");
        DownloadUtils.showDownloadManager(mActivity, mTab);
    }

    @Override
    public void navigateToHelpPage() {
        NewTabPageUma.recordAction(NewTabPageUma.ACTION_CLICKED_LEARN_MORE);
        String url = "https://support.google.com/chrome/?p=new_tab";
        // TODO(mastiz): Change this to LINK?
        openUrl(WindowOpenDisposition.CURRENT_TAB,
                new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK));
    }

    @Override
    public void openSnippet(int windowOpenDisposition, SnippetArticle article) {
        NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_SNIPPET);

        if (article.mIsAssetDownload) {
            assert windowOpenDisposition == WindowOpenDisposition.CURRENT_TAB
                    || windowOpenDisposition == WindowOpenDisposition.NEW_WINDOW
                    || windowOpenDisposition == WindowOpenDisposition.NEW_FOREGROUND_TAB;
            DownloadUtils.openFile(
                    article.getAssetDownloadFile(), article.getAssetDownloadMimeType(), false);
            return;
        }

        if (article.isRecentTab()) {
            assert windowOpenDisposition == WindowOpenDisposition.CURRENT_TAB;
            // TODO(vitaliii): Add a debug check that the result is true after crbug.com/662924
            // is resolved.
            openRecentTabSnippet(article);
            return;
        }

        // TODO(treib): Also track other dispositions. crbug.com/665915
        if (windowOpenDisposition == WindowOpenDisposition.CURRENT_TAB) {
            NewTabPageUma.monitorContentSuggestionVisit(mTab, article.mCategory);
        }

        LoadUrlParams loadUrlParams;
        // We explicitly open an offline page only for offline page downloads. For all other
        // sections the URL is opened and it is up to Offline Pages whether to open its offline
        // page (e.g. when offline).
        if (article.isDownload() && !article.mIsAssetDownload) {
            assert article.getOfflinePageOfflineId() != null;
            assert windowOpenDisposition == WindowOpenDisposition.CURRENT_TAB
                    || windowOpenDisposition == WindowOpenDisposition.NEW_WINDOW
                    || windowOpenDisposition == WindowOpenDisposition.NEW_FOREGROUND_TAB;
            loadUrlParams = OfflinePageUtils.getLoadUrlParamsForOpeningOfflineVersion(
                    article.mUrl, article.getOfflinePageOfflineId());
            // Extra headers are not read in loadUrl, but verbatim headers are.
            loadUrlParams.setVerbatimHeaders(loadUrlParams.getExtraHeadersString());
        } else {
            loadUrlParams = new LoadUrlParams(article.mUrl, PageTransition.AUTO_BOOKMARK);
        }

        // For article suggestions, we set the referrer. This is exploited
        // to filter out these history entries for NTP tiles.
        // TODO(mastiz): Extend this with support for other categories.
        if (article.mCategory == KnownCategories.ARTICLES) {
            loadUrlParams.setReferrer(new Referrer(
                    CHROME_CONTENT_SUGGESTIONS_REFERRER, Referrer.REFERRER_POLICY_ALWAYS));
        }

        openUrl(windowOpenDisposition, loadUrlParams);
    }

    @Override
    public void openUrl(int windowOpenDisposition, LoadUrlParams loadUrlParams) {
        switch (windowOpenDisposition) {
            case WindowOpenDisposition.CURRENT_TAB:
                mTab.loadUrl(loadUrlParams);
                break;
            case WindowOpenDisposition.NEW_FOREGROUND_TAB:
                openUrlInNewTab(loadUrlParams, false);
                break;
            case WindowOpenDisposition.OFF_THE_RECORD:
                openUrlInNewTab(loadUrlParams, true);
                break;
            case WindowOpenDisposition.NEW_WINDOW:
                openUrlInNewWindow(loadUrlParams);
                break;
            case WindowOpenDisposition.SAVE_TO_DISK:
                saveUrlForOffline(loadUrlParams.getUrl());
                break;
            default:
                assert false;
        }
    }

    private boolean openRecentTabSnippet(SnippetArticle article) {
        TabModel tabModel = mTabModelSelector.getModel(false);
        int tabId = Integer.parseInt(article.getRecentTabId());
        int tabIndex = TabModelUtils.getTabIndexById(tabModel, tabId);
        if (tabIndex == TabModel.INVALID_TAB_INDEX) return false;
        TabModelUtils.setIndex(tabModel, tabIndex);
        return true;
    }

    private void openUrlInNewWindow(LoadUrlParams loadUrlParams) {
        TabDelegate tabDelegate = new TabDelegate(false);
        tabDelegate.createTabInOtherWindow(loadUrlParams, mActivity, mTab.getParentId());
    }

    private void openUrlInNewTab(LoadUrlParams loadUrlParams, boolean incognito) {
        mTabModelSelector.openNewTab(
                loadUrlParams, TabLaunchType.FROM_LONGPRESS_BACKGROUND, mTab, incognito);
    }

    private void saveUrlForOffline(String url) {
        OfflinePageNotificationBridge.showDownloadingToast(mActivity);
        OfflinePageBridge.getForProfile(mProfile).savePageLater(
                url, "ntp_suggestions", true /* userRequested */);
    }
}
