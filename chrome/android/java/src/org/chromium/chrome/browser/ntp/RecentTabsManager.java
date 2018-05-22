// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.accounts.Account;
import android.content.Context;
import android.graphics.Bitmap;
import android.support.annotation.IntDef;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.invalidation.InvalidationController;
import org.chromium.chrome.browser.ntp.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.ntp.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninAccessPoint;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.signin.SigninPromoController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.AndroidSyncSettings.AndroidSyncSettingsObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.List;

/**
 * Provides the domain logic and data for RecentTabsPage and RecentTabsRowAdapter.
 */
public class RecentTabsManager implements AndroidSyncSettingsObserver, SignInStateObserver,
                                          ProfileDataCache.Observer, AccountsChangeObserver {
    /**
     * Implement this to receive updates when the page contents change.
     */
    interface UpdatedCallback {
        /**
         * Called when the list of recently closed tabs or foreign sessions changes.
         */
        void onUpdated();
    }
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({PromoState.PROMO_NONE, PromoState.PROMO_SIGNIN_PERSONALIZED, PromoState.PROMO_SYNC})
    @interface PromoState {
        int PROMO_NONE = 0;
        int PROMO_SIGNIN_PERSONALIZED = 1;
        int PROMO_SYNC = 2;
    }

    private static final int RECENTLY_CLOSED_MAX_TAB_COUNT = 5;

    private static RecentlyClosedTabManager sRecentlyClosedTabManagerForTests;

    private final Profile mProfile;
    private final Tab mTab;
    private final Context mContext;

    private FaviconHelper mFaviconHelper;
    private ForeignSessionHelper mForeignSessionHelper;
    private List<ForeignSession> mForeignSessions;
    private List<RecentlyClosedTab> mRecentlyClosedTabs;
    private RecentTabsPagePrefs mPrefs;
    private RecentlyClosedTabManager mRecentlyClosedTabManager;
    private SigninManager mSignInManager;
    private UpdatedCallback mUpdatedCallback;
    private boolean mIsDestroyed;

    private final ProfileDataCache mProfileDataCache;
    private final SigninPromoController mSigninPromoController;

    /**
     * Create an RecentTabsManager to be used with RecentTabsPage and RecentTabsRowAdapter.
     *
     * @param tab The Tab that is showing this recent tabs page.
     * @param profile Profile that is associated with the current session.
     * @param context the Android context this manager will work in.
     */
    public RecentTabsManager(Tab tab, Profile profile, Context context) {
        mProfile = profile;
        mTab = tab;
        mForeignSessionHelper = new ForeignSessionHelper(profile);
        mPrefs = new RecentTabsPagePrefs(profile);
        mFaviconHelper = new FaviconHelper();
        mRecentlyClosedTabManager = sRecentlyClosedTabManagerForTests != null
                ? sRecentlyClosedTabManagerForTests
                : new RecentlyClosedBridge(profile);
        mSignInManager = SigninManager.get();
        mContext = context;

        int imageSize = context.getResources().getDimensionPixelSize(R.dimen.user_picture_size);
        mProfileDataCache = new ProfileDataCache(mContext, imageSize);
        mSigninPromoController = new SigninPromoController(SigninAccessPoint.RECENT_TABS);

        mRecentlyClosedTabManager.setTabsUpdatedRunnable(() -> {
            updateRecentlyClosedTabs();
            postUpdate();
        });

        updateRecentlyClosedTabs();
        registerForForeignSessionUpdates();
        updateForeignSessions();
        mForeignSessionHelper.triggerSessionSync();
        registerObservers();

        InvalidationController.get(mContext).onRecentTabsPageOpened();
    }

    /**
     * Should be called when this object is no longer needed. Performs necessary listener tear down.
     */
    public void destroy() {
        mIsDestroyed = true;
        AndroidSyncSettings.unregisterObserver(mContext, this);

        mSignInManager.removeSignInStateObserver(this);
        mSignInManager = null;

        mProfileDataCache.removeObserver(this);
        AccountManagerFacade.get().removeObserver(this);

        mFaviconHelper.destroy();
        mFaviconHelper = null;

        mRecentlyClosedTabManager.destroy();
        mRecentlyClosedTabManager = null;

        mForeignSessionHelper.destroy();
        mForeignSessionHelper = null;

        mUpdatedCallback = null;

        mPrefs.destroy();
        mPrefs = null;

        InvalidationController.get(mContext).onRecentTabsPageClosed();
    }

    private void registerForForeignSessionUpdates() {
        mForeignSessionHelper.setOnForeignSessionCallback(() -> {
            updateForeignSessions();
            postUpdate();
        });
    }

    private void registerObservers() {
        AndroidSyncSettings.registerObserver(mContext, this);
        mSignInManager.addSignInStateObserver(this);

        mProfileDataCache.addObserver(this);
        AccountManagerFacade.get().addObserver(this);
    }

    private void updateRecentlyClosedTabs() {
        mRecentlyClosedTabs =
                mRecentlyClosedTabManager.getRecentlyClosedTabs(RECENTLY_CLOSED_MAX_TAB_COUNT);
    }

    private void updateForeignSessions() {
        mForeignSessions = mForeignSessionHelper.getForeignSessions();
        if (mForeignSessions == null) {
            mForeignSessions = Collections.emptyList();
        }
    }

    /**
     * @return Most up-to-date list of foreign sessions.
     */
    public List<ForeignSession> getForeignSessions() {
        return mForeignSessions;
    }

    /**
     * @return Most up-to-date list of recently closed tabs.
     */
    public List<RecentlyClosedTab> getRecentlyClosedTabs() {
        return mRecentlyClosedTabs;
    }

    /**
     * Opens a new tab navigating to ForeignSessionTab.
     *
     * @param session The foreign session that the tab belongs to.
     * @param tab The tab to open.
     * @param windowDisposition The WindowOpenDisposition flag.
     */
    public void openForeignSessionTab(ForeignSession session, ForeignSessionTab tab,
            int windowDisposition) {
        if (mIsDestroyed) return;
        RecordUserAction.record("MobileRecentTabManagerTabFromOtherDeviceOpened");
        mForeignSessionHelper.openForeignSessionTab(mTab, session, tab, windowDisposition);
    }

    /**
     * Restores a recently closed tab.
     *
     * @param tab The tab to open.
     * @param windowDisposition The WindowOpenDisposition value specifying whether the tab should
     *         be restored into the current tab or a new tab.
     */
    public void openRecentlyClosedTab(RecentlyClosedTab tab, int windowDisposition) {
        if (mIsDestroyed) return;
        RecordUserAction.record("MobileRecentTabManagerRecentTabOpened");
        mRecentlyClosedTabManager.openRecentlyClosedTab(mTab, tab, windowDisposition);
    }

    /**
     * Opens the history page.
     */
    public void openHistoryPage() {
        if (mIsDestroyed) return;
        HistoryManagerUtils.showHistoryManager(mTab.getActivity(), mTab);
    }

    /**
     * Returns a 16x16 favicon for a given synced url.
     *
     * @param url The url to fetch the favicon for.
     * @return 16x16 favicon or null if favicon unavailable.
     */
    public Bitmap getSyncedFaviconImageForURL(String url) {
        return mFaviconHelper.getSyncedFaviconImageForURL(mProfile, url);
    }

    /**
     * Fetches a favicon for snapshot document url which is returned via callback.
     *
     * @param url The url to fetch a favicon for.
     * @param size the desired favicon size.
     * @param faviconCallback the callback to be invoked when the favicon is available.
     *
     * @return may return false if we could not fetch the favicon.
     */
    public boolean getLocalFaviconForUrl(String url, int size,
            FaviconImageCallback faviconCallback) {
        return mFaviconHelper.getLocalFaviconImageForURL(mProfile, url, size, faviconCallback);
    }

    /**
     * Sets a callback to be invoked when recently closed tabs or foreign sessions documents have
     * been updated.
     *
     * @param updatedCallback the listener to be invoked.
     */
    public void setUpdatedCallback(UpdatedCallback updatedCallback) {
        mUpdatedCallback = updatedCallback;
    }

    /**
     * Sets the persistent expanded/collapsed state of a foreign session list.
     *
     * @param session foreign session to collapsed.
     * @param isCollapsed Whether the session is collapsed or expanded.
     */
    public void setForeignSessionCollapsed(ForeignSession session, boolean isCollapsed) {
        if (mIsDestroyed) return;
        mPrefs.setForeignSessionCollapsed(session, isCollapsed);
    }

    /**
     * Determine the expanded/collapsed state of a foreign session list.
     *
     * @param session foreign session whose state to obtain.
     *
     * @return Whether the session is collapsed.
     */
    public boolean getForeignSessionCollapsed(ForeignSession session) {
        return mPrefs.getForeignSessionCollapsed(session);
    }

    /**
     * Sets the persistent expanded/collapsed state of the recently closed tabs list.
     *
     * @param isCollapsed Whether the recently closed tabs list is collapsed.
     */
    public void setRecentlyClosedTabsCollapsed(boolean isCollapsed) {
        if (mIsDestroyed) return;
        mPrefs.setRecentlyClosedTabsCollapsed(isCollapsed);
    }

    /**
     * Determine the expanded/collapsed state of the recently closed tabs list.
     *
     * @return Whether the recently closed tabs list is collapsed.
     */
    public boolean isRecentlyClosedTabsCollapsed() {
        return mPrefs.getRecentlyClosedTabsCollapsed();
    }

    /**
     * Remove Foreign session to display. Note that it might reappear during the next sync if the
     * session is not orphaned.
     *
     * This is mainly for when user wants to delete an orphaned session.
     * @param session Session to be deleted.
     */
    public void deleteForeignSession(ForeignSession session) {
        if (mIsDestroyed) return;
        mForeignSessionHelper.deleteForeignSession(session);
    }

    /**
     * Clears the list of recently closed tabs.
     */
    public void clearRecentlyClosedTabs() {
        if (mIsDestroyed) return;
        mRecentlyClosedTabManager.clearRecentlyClosedTabs();
    }

    /**
     * Collapse the promo.
     *
     * @param isCollapsed Whether the promo is collapsed.
     */
    public void setPromoCollapsed(boolean isCollapsed) {
        if (mIsDestroyed) return;
        mPrefs.setSyncPromoCollapsed(isCollapsed);
    }

    /**
     * Determine whether the promo is collapsed.
     *
     * @return Whether the promo is collapsed.
     */
    public boolean isPromoCollapsed() {
        return mPrefs.getSyncPromoCollapsed();
    }

    /**
     * @return The promo type that will be displayed on the screen.
     */
    @PromoState
    int getPromoType() {
        if (!ChromeSigninController.get().isSignedIn()) {
            if (!SigninManager.get().isSignInAllowed()) {
                return PromoState.PROMO_NONE;
            }
            return PromoState.PROMO_SIGNIN_PERSONALIZED;
        }

        if (AndroidSyncSettings.isSyncEnabled(mContext)
                && AndroidSyncSettings.isChromeSyncEnabled(mContext)
                && !mForeignSessions.isEmpty()) {
            return PromoState.PROMO_NONE;
        }
        return PromoState.PROMO_SYNC;
    }

    void recordRecentTabMetrics() {
        RecordHistogram.recordCountHistogram(
                "Android.RecentTabsManager.RecentlyClosedTabs", mRecentlyClosedTabs.size());
        RecordHistogram.recordCountHistogram(
                "Android.RecentTabsManager.OtherDevices", mForeignSessions.size());

        int totalCount = mRecentlyClosedTabs.size();
        for (int i = 0; i < mForeignSessions.size(); i++) {
            ForeignSession foreignSession = mForeignSessions.get(i);
            for (int j = 0; j < foreignSession.windows.size(); j++) {
                totalCount += foreignSession.windows.get(j).tabs.size();
            }
        }
        RecordHistogram.recordCountHistogram("Android.RecentTabsManager.TotalTabs", totalCount);
    }

    /**
     * Sets up the personalized signin promo and records user actions for promo impressions.
     * @param view The view to be configured.
     */
    void setupPersonalizedSigninPromo(PersonalizedSigninPromoView view) {
        DisplayableProfileData profileData = null;
        Account[] accounts = AccountManagerFacade.get().tryGetGoogleAccounts();
        if (accounts.length > 0) {
            String defaultAccountName = accounts[0].name;
            mProfileDataCache.update(Collections.singletonList(defaultAccountName));
            profileData = mProfileDataCache.getProfileDataOrDefault(defaultAccountName);
        }
        mSigninPromoController.detach();
        mSigninPromoController.setupPromoView(mContext, view, profileData, null);
    }

    // SignInStateObserver implementation.
    @Override
    public void onSignedIn() {
        update();
    }

    @Override
    public void onSignedOut() {
        update();
    }

    // AccountsChangeObserver implementation.
    @Override
    public void onAccountsChanged() {
        update();
    }

    // ProfileDataCache.Observer implementation.
    @Override
    public void onProfileDataUpdated(String accountId) {
        update();
    }

    // AndroidSyncSettingsObserver implementation.
    @Override
    public void androidSyncSettingsChanged() {
        update();
    }

    private void postUpdate() {
        if (mUpdatedCallback != null) {
            mUpdatedCallback.onUpdated();
        }
    }

    private void update() {
        ThreadUtils.runOnUiThread(() -> {
            if (mIsDestroyed) return;
            updateForeignSessions();
            postUpdate();
        });
    }

    @VisibleForTesting
    public static void setRecentlyClosedTabManagerForTests(RecentlyClosedTabManager manager) {
        sRecentlyClosedTabManagerForTests = manager;
    }
}
