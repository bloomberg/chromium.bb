// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.Dialog;
import android.graphics.Bitmap;
import android.os.Build;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem.OnMenuItemClickListener;
import android.view.View;

import com.google.android.apps.chrome.R;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.NativePage;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.document.DocumentMetricIds;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarkUtils;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.ntp.BookmarksPage.BookmarkSelectedListener;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.preferences.DocumentModeManager;
import org.chromium.chrome.browser.preferences.DocumentModePreference;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.profiles.MostVisitedSites;
import org.chromium.chrome.browser.profiles.MostVisitedSites.MostVisitedURLsObserver;
import org.chromium.chrome.browser.profiles.MostVisitedSites.ThumbnailCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

/**
 * Provides functionality when the user interacts with the most visited websites page.
 */
public class DocumentNewTabPage implements NativePage {
    private static final int ID_OPEN_IN_NEW_TAB = 0;
    private static final int ID_OPEN_IN_INCOGNITO_TAB = 1;
    private static final int ID_REMOVE = 2;

    // The number of times that an opt-out promo will be shown.
    private static final int MAX_OPT_OUT_PROMO_COUNT = 10;

    private final Tab mTab;
    private final TabModelSelector mTabModelSelector;
    private final Activity mActivity;

    private final Profile mProfile;
    private final String mTitle;
    private final int mBackgroundColor;
    private final NewTabPageView mNewTabPageView;

    private MostVisitedSites mMostVisitedSites;
    private FaviconHelper mFaviconHelper;
    private LargeIconBridge mLargeIconBridge;

    private final NewTabPageManager mNewTabPageManager = new NewTabPageManager() {
        private void recordOpenedMostVisitedItem(MostVisitedItem item) {
            NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_MOST_VISITED_ENTRY);
            RecordHistogram.recordEnumeratedHistogram("NewTabPage.MostVisited", item.getIndex(),
                    NewTabPageView.MAX_MOST_VISITED_SITES);
            mMostVisitedSites.recordOpenedMostVisitedItem(item.getIndex());
        }

        private void recordDocumentOptOutPromoClick(int which) {
            RecordHistogram.recordEnumeratedHistogram("DocumentActivity.OptOutClick", which,
                    DocumentMetricIds.OPT_OUT_CLICK_COUNT);
        }

        @Override
        public void open(MostVisitedItem item) {
            String url = item.getUrl();
            mTab.loadUrl(new LoadUrlParams(url, PageTransition.LINK));
        }

        @Override
        public void onCreateContextMenu(ContextMenu menu, OnMenuItemClickListener listener) {
            menu.add(Menu.NONE, ID_OPEN_IN_NEW_TAB, Menu.NONE, R.string.contextmenu_open_in_new_tab)
                    .setOnMenuItemClickListener(listener);
            if (PrefServiceBridge.getInstance().isIncognitoModeEnabled()) {
                menu.add(Menu.NONE, ID_OPEN_IN_INCOGNITO_TAB, Menu.NONE,
                        R.string.contextmenu_open_in_incognito_tab).setOnMenuItemClickListener(
                        listener);
            }
            menu.add(Menu.NONE, ID_REMOVE, Menu.NONE, R.string.remove)
                    .setOnMenuItemClickListener(listener);
        }

        @Override
        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        public boolean onMenuItemClick(int menuId, MostVisitedItem item) {
            switch (menuId) {
                case ID_OPEN_IN_NEW_TAB:
                    recordOpenedMostVisitedItem(item);
                    mTabModelSelector.openNewTab(new LoadUrlParams(item.getUrl()),
                            TabLaunchType.FROM_LONGPRESS_BACKGROUND, mTab, false);
                    return true;
                case ID_OPEN_IN_INCOGNITO_TAB:
                    recordOpenedMostVisitedItem(item);
                    mActivity.finishAndRemoveTask();
                    mTabModelSelector.openNewTab(new LoadUrlParams(item.getUrl()),
                            TabLaunchType.FROM_LONGPRESS_FOREGROUND, mTab, true);
                    return true;
                case ID_REMOVE:
                    mMostVisitedSites.blacklistUrl(item.getUrl());
                    return true;
                default:
                    return false;
            }
        }

        @Override
        public void setMostVisitedURLsObserver(MostVisitedURLsObserver observer, int numResults) {
            mMostVisitedSites.setMostVisitedURLsObserver(observer, numResults);
        }

        @Override
        public void getURLThumbnail(String url, ThumbnailCallback thumbnailCallback) {
            mMostVisitedSites.getURLThumbnail(url, thumbnailCallback);
        }

        @Override
        public void getLocalFaviconImageForURL(
                String url, int size, FaviconImageCallback faviconCallback) {
            if (mFaviconHelper == null) mFaviconHelper = new FaviconHelper();
            mFaviconHelper.getLocalFaviconImageForURL(mProfile, url, FaviconHelper.FAVICON
                    | FaviconHelper.TOUCH_ICON | FaviconHelper.TOUCH_PRECOMPOSED_ICON, size,
                    faviconCallback);
        }

        @Override
        public void getLargeIconForUrl(String url, int size, LargeIconCallback callback) {
            if (mLargeIconBridge == null) mLargeIconBridge = new LargeIconBridge();
            mLargeIconBridge.getLargeIconForUrl(mProfile, url, size, callback);
        }

        @Override
        public void onLoadingComplete() {
        }

        @Override
        public void navigateToBookmarks() {
            RecordUserAction.record("MobileNTPSwitchToBookmarks");
            launchBookmarksDialog(mActivity, mTab, mTabModelSelector);
        }

        @Override
        public void navigateToRecentTabs() {
            RecordUserAction.record("MobileNTPSwitchToOpenTabs");
            launchRecentTabsDialog(mActivity, mTab, true);
        }

        @Override
        public boolean isLocationBarShownInNTP() {
            return false;
        }

        @Override
        public void focusSearchBox(boolean beginVoiceSearch, String pastedText) {
        }

        @Override
        public void openLogoLink() {
        }

        @Override
        public void getSearchProviderLogo(LogoObserver logoObserver) {
        }

        @Override
        public boolean shouldShowOptOutPromo() {
            DocumentModeManager documentModeManager = DocumentModeManager.getInstance(mActivity);
            return !documentModeManager.isOptOutPromoDismissed()
                    && (documentModeManager.getOptOutShownCount() < MAX_OPT_OUT_PROMO_COUNT);
        }

        @Override
        public void optOutPromoShown() {
            DocumentModeManager.getInstance(mActivity).incrementOptOutShownCount();
            RecordUserAction.record("DocumentActivity_OptOutShownOnHome");
        }

        @Override
        public void optOutPromoClicked(boolean settingsClicked) {
            if (settingsClicked) {
                recordDocumentOptOutPromoClick(DocumentMetricIds.OPT_OUT_CLICK_SETTINGS);
                PreferencesLauncher.launchSettingsPage(mActivity,
                        DocumentModePreference.class.getName());
            } else {
                recordDocumentOptOutPromoClick(DocumentMetricIds.OPT_OUT_CLICK_GOT_IT);
                DocumentModeManager documentModeManager = DocumentModeManager.getInstance(
                        mActivity);
                documentModeManager.setOptedOutState(DocumentModeManager.OPT_OUT_PROMO_DISMISSED);
            }
        }
    };

    /**
     * Constructs a NewTabPage.
     * @param activity Activity that owns the page.
     * @param tab Tab displayed on the page.
     */
    public DocumentNewTabPage(Activity activity, Tab tab, TabModelSelector tabModelSelector) {
        mTab = tab;
        mActivity = activity;
        mTabModelSelector = tabModelSelector;
        mProfile = tab.getProfile().getOriginalProfile();

        mTitle = activity.getResources().getString(R.string.button_new_tab);
        mBackgroundColor = activity.getResources().getColor(R.color.ntp_bg);

        mMostVisitedSites = new MostVisitedSites(mProfile);

        LayoutInflater inflater = LayoutInflater.from(activity);
        mNewTabPageView = (NewTabPageView) inflater.inflate(R.layout.new_tab_page, null);
        mNewTabPageView.initialize(mNewTabPageManager, false, false);
    }

    @Override
    public View getView() {
        return mNewTabPageView;
    }

    @Override
    public void destroy() {
        assert getView().getParent() == null : "Destroy called before removed from window";
        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
            mFaviconHelper = null;
        }
        if (mLargeIconBridge != null) {
            mLargeIconBridge.destroy();
            mLargeIconBridge = null;
        }
        if (mMostVisitedSites != null) {
            mMostVisitedSites.destroy();
            mMostVisitedSites = null;
        }
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public String getUrl() {
        return UrlConstants.NTP_URL;
    }

    @Override
    public String getHost() {
        return UrlConstants.NTP_HOST;
    }

    @Override
    public int getBackgroundColor() {
        return mBackgroundColor;
    }

    @Override
    public void updateForUrl(String url) {
    }

    private static class BookmarkDialogSelectedListener implements BookmarkSelectedListener {
        private Dialog mDialog;
        private final Tab mTab;

        public BookmarkDialogSelectedListener(Tab tab) {
            mTab = tab;
        }

        @Override
        public void onNewTabOpened() {
            if (mDialog != null) mDialog.dismiss();
        }

        @Override
        public void onBookmarkSelected(String url, String title, Bitmap favicon) {
            if (mDialog != null) mDialog.dismiss();
            mTab.loadUrl(new LoadUrlParams(url));
        }

        public void setDialog(Dialog dialog) {
            mDialog = dialog;
        }
    }

    public static void launchBookmarksDialog(Activity activity, Tab tab,
            TabModelSelector tabModelSelector) {
        if (!EnhancedBookmarkUtils.showEnhancedBookmarkIfEnabled(activity)) {
            BookmarkDialogSelectedListener listener = new BookmarkDialogSelectedListener(tab);
            NativePage page = BookmarksPage.buildPageInDocumentMode(
                    activity, tab, tabModelSelector, Profile.getLastUsedProfile(),
                    listener);
            page.updateForUrl(UrlConstants.BOOKMARKS_URL);
            Dialog dialog = new NativePageDialog(activity, page);
            listener.setDialog(dialog);
            dialog.show();
        }
    }

    public static void launchRecentTabsDialog(Activity activity, Tab tab,
            boolean finishActivityOnOpen) {
        DocumentRecentTabsManager manager =
                new DocumentRecentTabsManager(tab, activity, finishActivityOnOpen);
        NativePage page = new RecentTabsPage(activity, manager);
        page.updateForUrl(UrlConstants.RECENT_TABS_URL);
        Dialog dialog = new NativePageDialog(activity, page);
        manager.setDialog(dialog);
        dialog.show();
    }
}
