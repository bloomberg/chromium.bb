// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.drawable.Drawable;
import android.support.annotation.DimenRes;
import android.support.annotation.StringRes;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;

import javax.annotation.Nullable;

/**
 * A controller for configuring the sign in promo. It sets up the sign in promo depending on the
 * context: whether there are any Google accounts on the device which have been previously signed in
 * or not.
 */
public class SigninPromoController {
    /**
     * Receives notifications when user clicks close button in the promo.
     */
    public interface OnDismissListener {
        /**
         * Action to be performed when the promo is being dismissed.
         */
        void onDismiss();
    }

    private static final String SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS =
            "signin_promo_impressions_count_bookmarks";
    private static final String SIGNIN_PROMO_IMPRESSIONS_COUNT_SETTINGS =
            "signin_promo_impressions_count_settings";
    private static final int MAX_IMPRESSIONS_BOOKMARKS = 20;
    private static final int MAX_IMPRESSIONS_SETTINGS = 5;

    private String mAccountName;
    private final ProfileDataCache mProfileDataCache;
    private final @AccountSigninActivity.AccessPoint int mAccessPoint;
    private final @Nullable String mImpressionCountName;
    private final String mImpressionUserActionName;
    private final String mImpressionWithAccountUserActionName;
    private final String mImpressionWithNoAccountUserActionName;
    private final @StringRes int mDescriptionStringId;

    /**
     * Determines whether the promo should be shown to the user or not.
     * @param accessPoint The access point where the promo will be shown.
     * @return true if the promo is to be shown and false otherwise.
     */
    public static boolean shouldShowPromo(@AccountSigninActivity.AccessPoint int accessPoint) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_SIGNIN_PROMOS)) {
            return false;
        }

        SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
        switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                return sharedPreferences.getInt(SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS, 0)
                        < MAX_IMPRESSIONS_BOOKMARKS;
            case SigninAccessPoint.NTP_CONTENT_SUGGESTIONS:
                // There is no impression limit for NTP content suggestions.
                return true;
            case SigninAccessPoint.RECENT_TABS:
                // There is no impression limit for Recent Tabs.
                return true;
            case SigninAccessPoint.SETTINGS:
                return sharedPreferences.getInt(SIGNIN_PROMO_IMPRESSIONS_COUNT_SETTINGS, 0)
                        < MAX_IMPRESSIONS_SETTINGS;
            default:
                assert false : "Unexpected value for access point: " + accessPoint;
                return false;
        }
    }

    /**
     * Creates a new SigninPromoController.
     * @param profileDataCache The profile data cache for the latest used account on the device.
     * @param accessPoint Specifies the AccessPoint from which the promo is to be shown.
     */
    public SigninPromoController(
            ProfileDataCache profileDataCache, @AccountSigninActivity.AccessPoint int accessPoint) {
        mProfileDataCache = profileDataCache;
        mAccessPoint = accessPoint;

        switch (mAccessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                mImpressionCountName = SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS;
                mImpressionUserActionName = "Signin_Impression_FromBookmarkManager";
                mImpressionWithAccountUserActionName =
                        "Signin_ImpressionWithAccount_FromBookmarkManager";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromBookmarkManager";
                mDescriptionStringId = R.string.signin_promo_description_bookmarks;
                break;
            case SigninAccessPoint.NTP_CONTENT_SUGGESTIONS:
                // There is no impression limit for NTP content suggestions.
                mImpressionCountName = null;
                mImpressionUserActionName = "Signin_Impression_FromNTPContentSuggestions";
                mImpressionWithAccountUserActionName =
                        "Signin_ImpressionWithAccount_FromNTPContentSuggestions";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromNTPContentSuggestions";
                mDescriptionStringId = R.string.signin_promo_description_ntp_content_suggestions;
                break;
            case SigninAccessPoint.RECENT_TABS:
                // There is no impression limit for Recent Tabs.
                mImpressionCountName = null;
                mImpressionUserActionName = "Signin_Impression_FromRecentTabs";
                mImpressionWithAccountUserActionName =
                        "Signin_ImpressionWithAccount_FromRecentTabs";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromRecentTabs";
                mDescriptionStringId = R.string.signin_promo_description_recent_tabs;
                break;
            case SigninAccessPoint.SETTINGS:
                mImpressionCountName = SIGNIN_PROMO_IMPRESSIONS_COUNT_SETTINGS;
                mImpressionUserActionName = "Signin_Impression_FromSettings";
                mImpressionWithAccountUserActionName = "Signin_ImpressionWithAccount_FromSettings";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromSettings";
                mDescriptionStringId = R.string.signin_promo_description_settings;
                break;
            default:
                throw new IllegalArgumentException(
                        "Unexpected value for access point: " + mAccessPoint);
        }
    }

    /**
     * Records user actions for promo impressions.
     */
    public void recordSigninPromoImpression() {
        RecordUserAction.record(mImpressionUserActionName);
        if (mAccountName == null) {
            RecordUserAction.record(mImpressionWithNoAccountUserActionName);
        } else {
            RecordUserAction.record(mImpressionWithAccountUserActionName);
        }

        // If mImpressionCountName is not null then we should record impressions.
        if (mImpressionCountName != null) {
            SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
            int numImpressions = preferences.getInt(mImpressionCountName, 0);
            preferences.edit().putInt(mImpressionCountName, numImpressions + 1).apply();
        }
    }

    /**
     * Configures the signin promo view.
     * @param view The view in which the promo will be added.
     * @param onDismissListener Listener which handles the action of dismissing the promo. A null
     *         onDismissListener marks that the promo is not dismissible and as a result the close
     *         button is hidden.
     */
    public void setupSigninPromoView(Context context, SigninPromoView view,
            @Nullable final OnDismissListener onDismissListener) {
        view.getDescription().setText(mDescriptionStringId);

        if (mAccountName == null) {
            setupColdState(context, view);
        } else {
            setupHotState(context, view);
        }

        if (onDismissListener != null) {
            view.getDismissButton().setVisibility(View.VISIBLE);
            view.getDismissButton().setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View view) {
                    onDismissListener.onDismiss();
                }
            });
        } else {
            view.getDismissButton().setVisibility(View.GONE);
        }
    }

    /**
     * Sets the the default account found on the device.
     * @param accountName The name of the default account found on the device. Can be null if there
     *         are no accounts signed in on the device.
     */
    public void setAccountName(@Nullable String accountName) {
        mAccountName = accountName;
    }

    /** @return the resource used for the text displayed as promo description. */
    public @StringRes int getDescriptionStringId() {
        return mDescriptionStringId;
    }

    private void setupColdState(final Context context, SigninPromoView view) {
        view.getImage().setImageResource(R.drawable.chrome_sync_logo);
        setImageSize(context, view, R.dimen.signin_promo_cold_state_image_size);

        view.getSigninButton().setText(R.string.sign_in_to_chrome);
        view.getSigninButton().setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                AccountSigninActivity.startFromAddAccountPage(context, mAccessPoint);
            }
        });

        view.getChooseAccountButton().setVisibility(View.GONE);
    }

    private void setupHotState(final Context context, SigninPromoView view) {
        Drawable accountImage = mProfileDataCache.getImage(mAccountName);
        view.getImage().setImageDrawable(accountImage);
        setImageSize(context, view, R.dimen.signin_promo_account_image_size);

        String accountTitle = getAccountTitle();
        String signinButtonText =
                context.getString(R.string.signin_promo_continue_as, accountTitle);
        view.getSigninButton().setText(signinButtonText);
        view.getSigninButton().setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                AccountSigninActivity.startFromConfirmationPage(
                        context, mAccessPoint, mAccountName, true);
            }
        });

        String chooseAccountButtonText =
                context.getString(R.string.signin_promo_choose_account, mAccountName);
        view.getChooseAccountButton().setText(chooseAccountButtonText);
        view.getChooseAccountButton().setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                AccountSigninActivity.startAccountSigninActivity(context, mAccessPoint);
            }
        });
        view.getChooseAccountButton().setVisibility(View.VISIBLE);
    }

    private void setImageSize(Context context, SigninPromoView view, @DimenRes int dimenResId) {
        ViewGroup.LayoutParams layoutParams = view.getImage().getLayoutParams();
        layoutParams.height = context.getResources().getDimensionPixelSize(dimenResId);
        layoutParams.width = context.getResources().getDimensionPixelSize(dimenResId);
        view.getImage().setLayoutParams(layoutParams);
    }

    private String getAccountTitle() {
        String title = mProfileDataCache.getFullName(mAccountName);
        return title == null ? mAccountName : title;
    }
}
