// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.accounts.Account;
import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.IntDef;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninAccessPoint;
import org.chromium.chrome.browser.signin.SigninAndSyncView;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.signin.SigninPromoController;
import org.chromium.chrome.browser.signin.SigninPromoView;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.AndroidSyncSettings.AndroidSyncSettingsObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;

import javax.annotation.Nullable;

/**
 * Class that manages all the logic and UI behind the signin promo header in the bookmark
 * content UI. The header is shown only on certain situations, (e.g., not signed in).
 */
class BookmarkPromoHeader implements AndroidSyncSettingsObserver, SignInStateObserver,
                                     ProfileDataCache.Observer, AccountsChangeObserver {
    /**
     * Specifies the various states in which the Bookmarks promo can be.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({PromoState.PROMO_NONE, PromoState.PROMO_SIGNIN_NEW, PromoState.PROMO_SIGNIN_OLD,
            PromoState.PROMO_SYNC})
    @interface PromoState {
        int PROMO_NONE = 0;
        int PROMO_SIGNIN_NEW = 1;
        int PROMO_SIGNIN_OLD = 2;
        int PROMO_SYNC = 3;
    }

    // New signin promo preferences.
    private static final String PREF_NEW_SIGNIN_PROMO_DECLINED = "signin_promo_bookmarks_declined";
    // Old signin promo preferences.
    private static final String PREF_OLD_SIGNIN_PROMO_DECLINED =
            "enhanced_bookmark_signin_promo_declined";
    private static final String PREF_SIGNIN_PROMO_SHOW_COUNT =
            "enhanced_bookmark_signin_promo_show_count";
    // TODO(kkimlabs): Figure out the optimal number based on UMA data.
    private static final int MAX_SIGNIN_PROMO_SHOW_COUNT = 10;

    private static @Nullable @PromoState Integer sPromoStateForTests;

    private final Context mContext;
    private final SigninManager mSignInManager;
    private final Runnable mPromoHeaderChangeAction;
    private final BookmarkDelegate mBookmarkDelegate;
    private SigninPromoController mSigninPromoController;
    private ProfileDataCache mProfileDataCache;
    private boolean mIsNewPromoShowing;
    private @PromoState int mPromoState;

    /**
     * Initializes the class. Note that this will start listening to signin related events and
     * update itself if needed.
     */
    BookmarkPromoHeader(
            Context context, Runnable promoHeaderChangeAction, BookmarkDelegate bookmarkDelegate) {
        mContext = context;
        mPromoHeaderChangeAction = promoHeaderChangeAction;
        mBookmarkDelegate = bookmarkDelegate;

        AndroidSyncSettings.registerObserver(mContext, this);

        if (SigninPromoController.shouldShowPromo(SigninAccessPoint.BOOKMARK_MANAGER)) {
            int imageSize =
                    mContext.getResources().getDimensionPixelSize(R.dimen.user_picture_size);
            mProfileDataCache =
                    new ProfileDataCache(mContext, Profile.getLastUsedProfile(), imageSize);
            mProfileDataCache.addObserver(this);
            mSigninPromoController = new SigninPromoController(
                    mProfileDataCache, SigninAccessPoint.BOOKMARK_MANAGER);
            AccountManagerFacade.get().addObserver(this);
        }

        mSignInManager = SigninManager.get(mContext);
        mSignInManager.addSignInStateObserver(this);

        mPromoState = calculatePromoState();
        if (mPromoState == PromoState.PROMO_SIGNIN_OLD || mPromoState == PromoState.PROMO_SYNC) {
            int promoShowCount =
                    ContextUtils.getAppSharedPreferences().getInt(PREF_SIGNIN_PROMO_SHOW_COUNT, 0);
            ContextUtils.getAppSharedPreferences()
                    .edit()
                    .putInt(PREF_SIGNIN_PROMO_SHOW_COUNT, promoShowCount + 1)
                    .apply();
            if (mPromoState == PromoState.PROMO_SIGNIN_OLD) {
                RecordUserAction.record("Signin_Impression_FromBookmarkManager");
            }
        }
    }

    /**
     * Clean ups the class.  Must be called once done using this class.
     */
    void destroy() {
        AndroidSyncSettings.unregisterObserver(mContext, this);

        if (mSigninPromoController != null) {
            AccountManagerFacade.get().removeObserver(this);
            mProfileDataCache.removeObserver(this);
            mSigninPromoController.onPromoDestroyed();
        }

        mSignInManager.removeSignInStateObserver(this);
    }

    /**
     * @return The current state of the promo.
     */
    @PromoState
    int getPromoState() {
        return mPromoState;
    }

    /**
     * @return New signin promo header {@link ViewHolder} instance that can be used with
     *         {@link RecyclerView}.
     */
    ViewHolder createNewPromoHolder(ViewGroup parent) {
        View view = LayoutInflater.from(mContext).inflate(
                R.layout.signin_promo_view_bookmarks, parent, false);

        // ViewHolder is abstract and it cannot be instantiated directly.
        return new ViewHolder(view) {};
    }

    /**
     * @return Singin and sync promo header {@link ViewHolder} instance that can be used with
     *         {@link RecyclerView}.
     */
    ViewHolder createOldPromoHolder(ViewGroup parent) {
        SigninAndSyncView.Listener listener = this ::setOldSigninPromoDeclined;

        SigninAndSyncView view =
                SigninAndSyncView.create(parent, listener, SigninAccessPoint.BOOKMARK_MANAGER);

        // ViewHolder is abstract and it cannot be instantiated directly.
        return new ViewHolder(view) {};
    }

    /**
     * Configures the new signin promo and records promo impressions.
     * @param view The view to be configured.
     */
    void setupSigninPromo(SigninPromoView view) {
        Account[] accounts = AccountManagerFacade.get().tryGetGoogleAccounts();
        String defaultAccountName = accounts.length == 0 ? null : accounts[0].name;

        if (defaultAccountName != null) {
            mProfileDataCache.update(Collections.singletonList(defaultAccountName));
        }

        mSigninPromoController.setAccountName(defaultAccountName);
        if (!mIsNewPromoShowing) {
            mIsNewPromoShowing = true;
            mSigninPromoController.recordSigninPromoImpression();
        }

        SigninPromoController.OnDismissListener listener = this ::setNewSigninPromoDeclined;
        mSigninPromoController.setupSigninPromoView(mContext, view, listener);
    }

    /**
     * Saves that the new promo was declined and updates the UI.
     */
    private void setNewSigninPromoDeclined() {
        SharedPreferences.Editor sharedPreferencesEditor =
                ContextUtils.getAppSharedPreferences().edit();
        sharedPreferencesEditor.putBoolean(PREF_NEW_SIGNIN_PROMO_DECLINED, true);
        sharedPreferencesEditor.apply();
        mPromoState = calculatePromoState();
        mPromoHeaderChangeAction.run();
    }

    /**
     * Saves that the old promo was dismissed and updates the UI.
     */
    private void setOldSigninPromoDeclined() {
        SharedPreferences.Editor sharedPreferencesEditor =
                ContextUtils.getAppSharedPreferences().edit();
        sharedPreferencesEditor.putBoolean(PREF_OLD_SIGNIN_PROMO_DECLINED, true);
        sharedPreferencesEditor.apply();
        mPromoState = calculatePromoState();
        mPromoHeaderChangeAction.run();
    }

    /**
     * @return Whether the user declined the new signin promo.
     */
    private boolean wasNewSigninPromoDeclined() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                PREF_NEW_SIGNIN_PROMO_DECLINED, false);
    }

    /**
     * @return Whether user tapped "No" button on the old signin promo.
     */
    private boolean wasOldSigninPromoDeclined() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                PREF_OLD_SIGNIN_PROMO_DECLINED, false);
    }

    private @PromoState int calculatePromoState() {
        if (sPromoStateForTests != null) {
            return sPromoStateForTests;
        }

        if (!AndroidSyncSettings.isMasterSyncEnabled(mContext)) {
            return PromoState.PROMO_NONE;
        }

        // If the user is signed in, then we should show the sync promo if Chrome sync is disabled
        // and the impression limit has not been reached yet.
        if (ChromeSigninController.get().isSignedIn()) {
            boolean impressionLimitNotReached =
                    ContextUtils.getAppSharedPreferences().getInt(PREF_SIGNIN_PROMO_SHOW_COUNT, 0)
                    < MAX_SIGNIN_PROMO_SHOW_COUNT;
            if (!AndroidSyncSettings.isChromeSyncEnabled(mContext) && impressionLimitNotReached) {
                return PromoState.PROMO_SYNC;
            }
            return PromoState.PROMO_NONE;
        }

        // The signin promo should be shown if signin is allowed, it wasn't declined by the user
        // and the impression limit wasn't reached.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_SIGNIN_PROMOS)) {
            if (mSignInManager.isSignInAllowed()
                    && SigninPromoController.shouldShowPromo(SigninAccessPoint.BOOKMARK_MANAGER)
                    && !wasNewSigninPromoDeclined()) {
                return PromoState.PROMO_SIGNIN_NEW;
            }
            return PromoState.PROMO_NONE;
        } else {
            boolean impressionLimitNotReached =
                    ContextUtils.getAppSharedPreferences().getInt(PREF_SIGNIN_PROMO_SHOW_COUNT, 0)
                    < MAX_SIGNIN_PROMO_SHOW_COUNT;
            if (mSignInManager.isSignInAllowed() && impressionLimitNotReached
                    && !wasOldSigninPromoDeclined()) {
                return PromoState.PROMO_SIGNIN_OLD;
            }
            return PromoState.PROMO_NONE;
        }
    }

    // AndroidSyncSettingsObserver implementation

    @Override
    public void androidSyncSettingsChanged() {
        mPromoState = calculatePromoState();
        mPromoHeaderChangeAction.run();
    }

    // SignInStateObserver implementations

    @Override
    public void onSignedIn() {
        mPromoState = calculatePromoState();
        mPromoHeaderChangeAction.run();
    }

    @Override
    public void onSignedOut() {
        mPromoState = calculatePromoState();
        mPromoHeaderChangeAction.run();
    }

    // ProfileDataCacheObserver implementation.

    @Override
    public void onProfileDataUpdated(String accountId) {
        mPromoHeaderChangeAction.run();
    }

    // AccountsChangeObserver implementation.

    @Override
    public void onAccountsChanged() {
        mPromoHeaderChangeAction.run();
    }

    /**
     * Forces the promo state to a particular value for testing purposes.
     * @param promoState The promo state to which the header will be set to.
     */
    @VisibleForTesting
    public static void forcePromoStateForTests(@PromoState int promoState) {
        sPromoStateForTests = promoState;
    }
}
