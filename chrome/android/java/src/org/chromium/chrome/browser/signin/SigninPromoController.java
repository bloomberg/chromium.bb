// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.drawable.Drawable;
import android.support.annotation.DimenRes;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.metrics.ImpressionTracker;
import org.chromium.chrome.browser.metrics.OneShotImpressionListener;
import org.chromium.chrome.browser.signin.AccountSigninActivity.AccessPoint;


/**
 * A controller for configuring the sign in promo. It sets up the sign in promo depending on the
 * context: whether there are any Google accounts on the device which have been previously signed in
 * or not. The controller also takes care of counting impressions, recording signin related user
 * actions and histograms.
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
    private static final int MAX_IMPRESSIONS_SETTINGS = 20;

    private @Nullable DisplayableProfileData mProfileData;
    private @Nullable ImpressionTracker mImpressionTracker;
    private final OneShotImpressionListener mImpressionFilter =
            new OneShotImpressionListener(this::recordSigninPromoImpression);
    private final @AccessPoint int mAccessPoint;
    private final @Nullable String mImpressionCountName;
    private final String mImpressionUserActionName;
    private final String mImpressionWithAccountUserActionName;
    private final String mImpressionWithNoAccountUserActionName;
    private final String mSigninWithDefaultUserActionName;
    private final String mSigninNotDefaultUserActionName;
    private final String mSigninNewAccountUserActionName;
    private final @Nullable String mImpressionsTilDismissHistogramName;
    private final @Nullable String mImpressionsTilSigninButtonsHistogramName;
    private final @Nullable String mImpressionsTilXButtonHistogramName;
    private final @StringRes int mDescriptionStringId;
    private boolean mWasDisplayed;
    private boolean mWasUsed;

    /**
     * Determines whether the impression limit has been reached for the given access point.
     * @param accessPoint The access point for which the impression limit is being checked.
     */
    public static boolean hasNotReachedImpressionLimit(@AccessPoint int accessPoint) {
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
     * @param accessPoint Specifies the AccessPoint from which the promo is to be shown.
     */
    public SigninPromoController(@AccessPoint int accessPoint) {
        mAccessPoint = accessPoint;

        switch (mAccessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                mImpressionCountName = SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS;
                mImpressionUserActionName = "Signin_Impression_FromBookmarkManager";
                mImpressionWithAccountUserActionName =
                        "Signin_ImpressionWithAccount_FromBookmarkManager";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromBookmarkManager";
                mSigninWithDefaultUserActionName = "Signin_SigninWithDefault_FromBookmarkManager";
                mSigninNotDefaultUserActionName = "Signin_SigninNotDefault_FromBookmarkManager";
                mSigninNewAccountUserActionName = "Signin_SigninNewAccount_FromBookmarkManager";
                mImpressionsTilDismissHistogramName =
                        "MobileSignInPromo.BookmarkManager.ImpressionsTilDismiss";
                mImpressionsTilSigninButtonsHistogramName =
                        "MobileSignInPromo.BookmarkManager.ImpressionsTilSigninButtons";
                mImpressionsTilXButtonHistogramName =
                        "MobileSignInPromo.BookmarkManager.ImpressionsTilXButton";
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
                mSigninWithDefaultUserActionName =
                        "Signin_SigninWithDefault_FromNTPContentSuggestions";
                mSigninNotDefaultUserActionName =
                        "Signin_SigninNotDefault_FromNTPContentSuggestions";
                mSigninNewAccountUserActionName =
                        "Signin_SigninNewAccount_FromNTPContentSuggestions";
                mImpressionsTilDismissHistogramName = null;
                mImpressionsTilSigninButtonsHistogramName = null;
                mImpressionsTilXButtonHistogramName = null;
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
                mSigninWithDefaultUserActionName = "Signin_SigninWithDefault_FromRecentTabs";
                mSigninNotDefaultUserActionName = "Signin_SigninNotDefault_FromRecentTabs";
                mSigninNewAccountUserActionName = "Signin_SigninNewAccount_FromRecentTabs";
                mImpressionsTilDismissHistogramName = null;
                mImpressionsTilSigninButtonsHistogramName = null;
                mImpressionsTilXButtonHistogramName = null;
                mDescriptionStringId = R.string.signin_promo_description_recent_tabs;
                break;
            case SigninAccessPoint.SETTINGS:
                mImpressionCountName = SIGNIN_PROMO_IMPRESSIONS_COUNT_SETTINGS;
                mImpressionUserActionName = "Signin_Impression_FromSettings";
                mImpressionWithAccountUserActionName = "Signin_ImpressionWithAccount_FromSettings";
                mSigninWithDefaultUserActionName = "Signin_SigninWithDefault_FromSettings";
                mSigninNotDefaultUserActionName = "Signin_SigninNotDefault_FromSettings";
                mSigninNewAccountUserActionName = "Signin_SigninNewAccount_FromSettings";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromSettings";
                mImpressionsTilDismissHistogramName =
                        "MobileSignInPromo.SettingsManager.ImpressionsTilDismiss";
                mImpressionsTilSigninButtonsHistogramName =
                        "MobileSignInPromo.SettingsManager.ImpressionsTilSigninButtons";
                mImpressionsTilXButtonHistogramName =
                        "MobileSignInPromo.SettingsManager.ImpressionsTilXButton";
                mDescriptionStringId = R.string.signin_promo_description_settings;
                break;
            default:
                throw new IllegalArgumentException(
                        "Unexpected value for access point: " + mAccessPoint);
        }
    }

    /**
     * Called when the signin promo is destroyed.
     */
    public void onPromoDestroyed() {
        if (!mWasDisplayed || mWasUsed || mImpressionsTilDismissHistogramName == null) {
            return;
        }
        RecordHistogram.recordCount100Histogram(
                mImpressionsTilDismissHistogramName, getNumImpressions());
    }

    /**
     * Configures the signin promo view and resets the impression tracker. If this controller has
     * been previously set up, it should be {@link #detach detached} first.
     * @param view The view in which the promo will be added.
     * @param profileData If not null, the promo will be configured to be in the hot state, using
     *         the account image, email and full name of the user to set the picture and the text of
     *         the promo appropriately. Otherwise, the promo will be in the cold state.
     * @param onDismissListener Listener which handles the action of dismissing the promo. A null
     *         onDismissListener marks that the promo is not dismissible and as a result the close
     *         button is hidden.
     */
    public void setupPromoView(Context context, PersonalizedSigninPromoView view,
            final @Nullable DisplayableProfileData profileData,
            final @Nullable OnDismissListener onDismissListener) {
        mProfileData = profileData;
        mWasDisplayed = true;

        assert mImpressionTracker == null : "detach() should be called before setting up a new " +
                "view";
        mImpressionTracker = new ImpressionTracker(view);
        mImpressionTracker.setListener(mImpressionFilter);

        view.getDescription().setText(mDescriptionStringId);

        if (mProfileData == null) {
            setupColdState(context, view);
        } else {
            setupHotState(context, view);
        }

        if (onDismissListener != null) {
            view.getDismissButton().setVisibility(View.VISIBLE);
            view.getDismissButton().setOnClickListener(promoView -> {
                assert mImpressionsTilXButtonHistogramName != null;
                mWasUsed = true;
                RecordHistogram.recordCount100Histogram(
                        mImpressionsTilXButtonHistogramName, getNumImpressions());
                onDismissListener.onDismiss();
            });
        } else {
            view.getDismissButton().setVisibility(View.GONE);
        }
    }

    /**
     * Should be called when the view is not in use anymore (e.g. it's being recycled).
     */
    public void detach() {
        if (mImpressionTracker != null) {
            mImpressionTracker.setListener(null);
            mImpressionTracker = null;
        }
    }

    /** @return the resource used for the text displayed as promo description. */
    public @StringRes int getDescriptionStringId() {
        return mDescriptionStringId;
    }

    private void setupColdState(final Context context, PersonalizedSigninPromoView view) {
        view.getImage().setImageResource(R.drawable.chrome_sync_logo);
        setImageSize(context, view, R.dimen.signin_promo_cold_state_image_size);

        view.getSigninButton().setText(R.string.sign_in_to_chrome);
        view.getSigninButton().setOnClickListener(v -> signinWithNewAccount(context));

        view.getChooseAccountButton().setVisibility(View.GONE);
    }

    private void setupHotState(final Context context, PersonalizedSigninPromoView view) {
        Drawable accountImage = mProfileData.getImage();
        view.getImage().setImageDrawable(accountImage);
        setImageSize(context, view, R.dimen.signin_promo_account_image_size);

        String signinButtonText = context.getString(
                R.string.signin_promo_continue_as, mProfileData.getFullNameOrEmail());
        view.getSigninButton().setText(signinButtonText);
        view.getSigninButton().setOnClickListener(v -> signinWithDefaultAccount(context));

        String chooseAccountButtonText = context.getString(
                R.string.signin_promo_choose_account, mProfileData.getAccountName());
        view.getChooseAccountButton().setText(chooseAccountButtonText);
        view.getChooseAccountButton().setOnClickListener(v -> signinWithNotDefaultAccount(context));
        view.getChooseAccountButton().setVisibility(View.VISIBLE);
    }

    private int getNumImpressions() {
        SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
        return preferences.getInt(mImpressionCountName, 0);
    }

    private void signinWithNewAccount(Context context) {
        recordSigninButtonUsed();
        RecordUserAction.record(mSigninNewAccountUserActionName);
        context.startActivity(AccountSigninActivity.createIntentForAddAccountSigninFlow(
                context, mAccessPoint, true));
    }

    private void signinWithDefaultAccount(Context context) {
        recordSigninButtonUsed();
        RecordUserAction.record(mSigninWithDefaultUserActionName);
        context.startActivity(AccountSigninActivity.createIntentForConfirmationOnlySigninFlow(
                context, mAccessPoint, mProfileData.getAccountName(), true, true));
    }

    private void signinWithNotDefaultAccount(Context context) {
        recordSigninButtonUsed();
        RecordUserAction.record(mSigninNotDefaultUserActionName);
        context.startActivity(AccountSigninActivity.createIntentForDefaultSigninFlow(
                context, mAccessPoint, true));
    }

    private void recordSigninButtonUsed() {
        mWasUsed = true;
        if (mImpressionsTilSigninButtonsHistogramName != null) {
            RecordHistogram.recordCount100Histogram(
                    mImpressionsTilSigninButtonsHistogramName, getNumImpressions());
        }
    }

    private void setImageSize(
            Context context, PersonalizedSigninPromoView view, @DimenRes int dimenResId) {
        ViewGroup.LayoutParams layoutParams = view.getImage().getLayoutParams();
        layoutParams.height = context.getResources().getDimensionPixelSize(dimenResId);
        layoutParams.width = context.getResources().getDimensionPixelSize(dimenResId);
        view.getImage().setLayoutParams(layoutParams);
    }

    private void recordSigninPromoImpression() {
        RecordUserAction.record(mImpressionUserActionName);
        if (mProfileData == null) {
            RecordUserAction.record(mImpressionWithNoAccountUserActionName);
        } else {
            RecordUserAction.record(mImpressionWithAccountUserActionName);
        }

        // If mImpressionCountName is not null then we should record impressions.
        if (mImpressionCountName != null) {
            SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
            int numImpressions = preferences.getInt(mImpressionCountName, 0) + 1;
            preferences.edit().putInt(mImpressionCountName, numImpressions).apply();
        }
    }

    @VisibleForTesting
    public static int getMaxImpressionsBookmarksForTests() {
        return MAX_IMPRESSIONS_BOOKMARKS;
    }
}
