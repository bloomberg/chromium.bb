// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.signin.SigninPromoController;
import org.chromium.chrome.browser.signin.SigninPromoUtil;
import org.chromium.chrome.browser.signin.SyncPromoView;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.AndroidSyncSettings.AndroidSyncSettingsObserver;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

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
    @IntDef({PromoState.PROMO_NONE, PromoState.PROMO_SIGNIN_PERSONALIZED, PromoState.PROMO_SYNC})
    @interface PromoState {
        int PROMO_NONE = 0;
        int PROMO_SIGNIN_PERSONALIZED = 1;
        int PROMO_SYNC = 2;
    }

    // TODO(kkimlabs): Figure out the optimal number based on UMA data.
    private static final int MAX_SIGNIN_AND_SYNC_PROMO_SHOW_COUNT = 10;

    private static @Nullable @PromoState Integer sPromoStateForTests;

    private final Context mContext;
    private final SigninManager mSignInManager;
    private final Runnable mPromoHeaderChangeAction;

    private final @Nullable ProfileDataCache mProfileDataCache;
    private final @Nullable SigninPromoController mSigninPromoController;
    private @PromoState int mPromoState;

    /**
     * Initializes the class. Note that this will start listening to signin related events and
     * update itself if needed.
     */
    BookmarkPromoHeader(Context context, Runnable promoHeaderChangeAction) {
        mContext = context;
        mPromoHeaderChangeAction = promoHeaderChangeAction;

        AndroidSyncSettings.get().registerObserver(this);

        if (SigninPromoController.hasNotReachedImpressionLimit(
                    SigninAccessPoint.BOOKMARK_MANAGER)) {
            int imageSize =
                    mContext.getResources().getDimensionPixelSize(R.dimen.user_picture_size);
            mProfileDataCache = new ProfileDataCache(mContext, imageSize);
            mProfileDataCache.addObserver(this);
            mSigninPromoController = new SigninPromoController(SigninAccessPoint.BOOKMARK_MANAGER);
            AccountManagerFacadeProvider.getInstance().addObserver(this);
        } else {
            mProfileDataCache = null;
            mSigninPromoController = null;
        }

        mSignInManager = IdentityServicesProvider.get().getSigninManager();
        mSignInManager.addSignInStateObserver(this);

        mPromoState = calculatePromoState();
        if (mPromoState == PromoState.PROMO_SYNC) {
            SharedPreferencesManager.getInstance().incrementInt(
                    ChromePreferenceKeys.SIGNIN_AND_SYNC_PROMO_SHOW_COUNT);
        }
    }

    /**
     * Clean ups the class. Must be called once done using this class.
     */
    void destroy() {
        AndroidSyncSettings.get().unregisterObserver(this);

        if (mSigninPromoController != null) {
            AccountManagerFacadeProvider.getInstance().removeObserver(this);
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
     * @return Personalized signin promo header {@link ViewHolder} instance that can be used with
     *         {@link RecyclerView}.
     */
    ViewHolder createPersonalizedSigninPromoHolder(ViewGroup parent) {
        View view = LayoutInflater.from(mContext).inflate(
                R.layout.personalized_signin_promo_view_bookmarks, parent, false);

        // ViewHolder is abstract and it cannot be instantiated directly.
        return new ViewHolder(view) {};
    }

    /**
     * @return Sync promo header {@link ViewHolder} instance that can be used with
     *         {@link RecyclerView}.
     */
    ViewHolder createSyncPromoHolder(ViewGroup parent) {
        SyncPromoView view = SyncPromoView.create(parent, SigninAccessPoint.BOOKMARK_MANAGER);

        // ViewHolder is abstract and it cannot be instantiated directly.
        return new ViewHolder(view) {};
    }

    /**
     * Configures the personalized signin promo and records promo impressions.
     * @param view The view to be configured.
     */
    void setupPersonalizedSigninPromo(PersonalizedSigninPromoView view) {
        SigninPromoUtil.setupPromoViewFromCache(mSigninPromoController, mProfileDataCache, view,
                this::setPersonalizedSigninPromoDeclined);
    }

    /**
     * Detaches the previously configured {@link PersonalizedSigninPromoView}.
     */
    void detachPersonalizePromoView() {
        if (mSigninPromoController != null) mSigninPromoController.detach();
    }

    /**
     * Saves that the personalized signin promo was declined and updates the UI.
     */
    private void setPersonalizedSigninPromoDeclined() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_PERSONALIZED_DECLINED, true);
        mPromoState = calculatePromoState();
        triggerPromoUpdate();
    }

    /**
     * @return Whether the user declined the personalized signin promo.
     */
    @VisibleForTesting
    static boolean wasPersonalizedSigninPromoDeclined() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_PERSONALIZED_DECLINED, false);
    }

    /**
     * @return Whether the personalized signin promo should be shown to user.
     */
    private boolean shouldShowBookmarkSigninPromo() {
        return mSignInManager.isSignInAllowed()
                && SigninPromoController.hasNotReachedImpressionLimit(
                        SigninAccessPoint.BOOKMARK_MANAGER)
                && !wasPersonalizedSigninPromoDeclined();
    }

    private @PromoState int calculatePromoState() {
        if (sPromoStateForTests != null) {
            return sPromoStateForTests;
        }

        if (!AndroidSyncSettings.get().isMasterSyncEnabled()) {
            return PromoState.PROMO_NONE;
        }

        if (!mSignInManager.getIdentityManager().hasPrimaryAccount()) {
            return shouldShowBookmarkSigninPromo() ? PromoState.PROMO_SIGNIN_PERSONALIZED
                                                   : PromoState.PROMO_NONE;
        }

        boolean impressionLimitNotReached =
                SharedPreferencesManager.getInstance().readInt(
                        ChromePreferenceKeys.SIGNIN_AND_SYNC_PROMO_SHOW_COUNT)
                < MAX_SIGNIN_AND_SYNC_PROMO_SHOW_COUNT;
        if (!AndroidSyncSettings.get().isChromeSyncEnabled() && impressionLimitNotReached) {
            return PromoState.PROMO_SYNC;
        }
        return PromoState.PROMO_NONE;
    }

    // AndroidSyncSettingsObserver implementation.
    @Override
    public void androidSyncSettingsChanged() {
        // AndroidSyncSettings calls this method from non-UI threads.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mPromoState = calculatePromoState();
            triggerPromoUpdate();
        });
    }

    // SignInStateObserver implementation.
    @Override
    public void onSignedIn() {
        mPromoState = calculatePromoState();
        triggerPromoUpdate();
    }

    @Override
    public void onSignedOut() {
        mPromoState = calculatePromoState();
        triggerPromoUpdate();
    }

    // ProfileDataCache.Observer implementation.
    @Override
    public void onProfileDataUpdated(String accountId) {
        triggerPromoUpdate();
    }

    // AccountsChangeObserver implementation.
    @Override
    public void onAccountsChanged() {
        triggerPromoUpdate();
    }

    private void triggerPromoUpdate() {
        detachPersonalizePromoView();
        mPromoHeaderChangeAction.run();
    }

    /**
     * Forces the promo state to a particular value for testing purposes.
     * @param promoState The promo state to which the header will be set to.
     */
    @VisibleForTesting
    static void forcePromoStateForTests(@Nullable @PromoState Integer promoState) {
        sPromoStateForTests = promoState;
    }

    @VisibleForTesting
    static void setPrefPersonalizedSigninPromoDeclinedForTests(boolean isDeclined) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_PERSONALIZED_DECLINED, isDeclined);
    }
}
