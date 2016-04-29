// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.signin.AccountSigninActivity.AccessPoint;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.ui.SyncCustomizationFragment;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.AndroidSyncSettings.AndroidSyncSettingsObserver;
import org.chromium.sync.signin.ChromeSigninController;

/**
 * A View that shows the user the next step they must complete to start syncing their data (eg.
 * Recent Tabs or Bookmarks). For example, if the user is not signed in, the View will prompt them
 * to do so and link to the AccountSigninActivity.
 */
public class SigninAndSyncView extends FrameLayout
        implements AndroidSyncSettingsObserver, SignInStateObserver {
    private final Listener mListener;
    @AccessPoint private final int mAccessPoint;
    private final SigninManager mSigninManager;

    private final TextView mTitle;
    private final TextView mDescription;
    private final Button mNegativeButton;
    private final Button mPositiveButton;

    /**
     * A listener for the container of the SigninAndSyncView to be informed of certain user
     * interactions.
     */
    public interface Listener {
        /**
         * The user has pressed 'no thanks' and expects the view to be removed from its parent.
         */
        public void onViewDismissed();
    }

    /**
     * Constructor for use from Java.
     */
    public SigninAndSyncView(Context context, Listener listener, @AccessPoint int accessPoint) {
        // TODO(peconn): Simplify BookmarkPromoHeader
        super(context);
        mListener = listener;
        mAccessPoint = accessPoint;

        assert mAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER
                || mAccessPoint == SigninAccessPoint.RECENT_TABS
                : "SigninAndSyncView only has strings for bookmark manager and recent tabs.";

        mSigninManager = SigninManager.get(getContext());

        addView(LayoutInflater.from(getContext())
                .inflate(R.layout.signin_and_sync_view, this, false));

        mTitle = (TextView) findViewById(R.id.title);
        mDescription = (TextView) findViewById(R.id.description);
        mNegativeButton = (Button) findViewById(R.id.no_thanks);
        mPositiveButton = (Button) findViewById(R.id.sign_in);

        // The title stays the same no matter what action the user must take.
        if (mAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER) {
            mTitle.setText(R.string.sync_your_bookmarks);
        } else {
            mTitle.setVisibility(View.GONE);
        }

        // We don't need to call update() here as it will be called in onAttachedToWindow().
    }

    private void update() {
        ViewState viewState;
        if (!ChromeSigninController.get(getContext()).isSignedIn()) {
            viewState = getStateForSignin();
        } else if (!AndroidSyncSettings.isMasterSyncEnabled(getContext())) {
            viewState = getStateForEnableAndroidSync();
        } else if (!AndroidSyncSettings.isChromeSyncEnabled(getContext())) {
            viewState = getStateForEnableChromeSync();
        } else {
            viewState = getStateForStartUsing();
        }
        viewState.apply(mDescription, mPositiveButton, mNegativeButton);
    }

    /**
     * The ViewState class represents all the UI elements that can change for each variation of
     * this View. We use this to ensure each variation (created in the getStateFor* methods)
     * explicitly touches each UI element.
     */
    private static class ViewState {
        private final int mDescriptionText;
        private final ButtonState mPositiveButtonState;
        private final ButtonState mNegativeButtonState;

        public ViewState(int mDescriptionText,
                ButtonState mPositiveButtonState, ButtonState mNegativeButtonState) {
            this.mDescriptionText = mDescriptionText;
            this.mPositiveButtonState = mPositiveButtonState;
            this.mNegativeButtonState = mNegativeButtonState;
        }

        public void apply(TextView description, Button positiveButton, Button negativeButton) {
            description.setText(mDescriptionText);
            mNegativeButtonState.apply(negativeButton);
            mPositiveButtonState.apply(positiveButton);
        }
    }

    /**
     * Classes to represent the state of a button that we are interested in, used to keep ViewState
     * tidy and provide some convenience methods.
     */
    private abstract static class ButtonState {
        public abstract void apply(Button button);
    }

    private static class ButtonAbsent extends ButtonState {
        @Override
        public void apply(Button button) {
            button.setVisibility(View.GONE);
        }
    }

    private static class ButtonPresent extends ButtonState {
        private final int mTextResource;
        private final OnClickListener mOnClickListener;

        public ButtonPresent(int textResource, OnClickListener onClickListener) {
            mTextResource = textResource;
            mOnClickListener = onClickListener;
        }

        @Override
        public void apply(Button button) {
            button.setVisibility(View.VISIBLE);
            button.setText(mTextResource);
            button.setOnClickListener(mOnClickListener);
        }
    }

    private ViewState getStateForSignin() {
        int descId = mAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER
                ? R.string.bookmark_sign_in_promo_description
                : R.string.recent_tabs_sign_in_promo_description;

        ButtonState positiveButton = new ButtonPresent(
                R.string.sign_in_button,
                new OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        AccountSigninActivity
                                .startAccountSigninActivity(getContext(), mAccessPoint);
                    }
                });

        ButtonState negativeButton;
        if (mAccessPoint == SigninAccessPoint.RECENT_TABS) {
            negativeButton = new ButtonAbsent();
        } else {
            negativeButton = new ButtonPresent(R.string.no_thanks, new OnClickListener() {
                @Override
                public void onClick(View view) {
                    mListener.onViewDismissed();
                }
            });
        }

        return new ViewState(descId, positiveButton, negativeButton);
    }

    private ViewState getStateForEnableAndroidSync() {
        assert mAccessPoint == SigninAccessPoint.RECENT_TABS
                : "Enable Android Sync should not be showing from bookmarks";

        int descId = R.string.recent_tabs_sync_promo_enable_android_sync;

        ButtonState positiveButton = new ButtonPresent(
                R.string.open_settings_button,
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        // TODO(crbug.com/557784): Like AccountManagementFragment, this would also
                        // benefit from going directly to an account.
                        Intent intent = new Intent(Settings.ACTION_SYNC_SETTINGS);
                        intent.putExtra(Settings.EXTRA_ACCOUNT_TYPES, new String[] {"com.google"});
                        getContext().startActivity(intent);
                    }
                });

        return new ViewState(descId, positiveButton, new ButtonAbsent());
    }

    private ViewState getStateForEnableChromeSync() {
        int descId = mAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER
                ? R.string.bookmarks_sync_promo_enable_sync
                : R.string.recent_tabs_sync_promo_enable_chrome_sync;

        ButtonState positiveButton = new ButtonPresent(
                R.string.enable_sync_button,
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        PreferencesLauncher.launchSettingsPage(getContext(),
                                SyncCustomizationFragment.class.getName());
                    }
                });

        return new ViewState(descId, positiveButton, new ButtonAbsent());
    }

    private ViewState getStateForStartUsing() {
        assert mAccessPoint == SigninAccessPoint.RECENT_TABS
                : "This should not be showing from bookmarks";

        return new ViewState(R.string.ntp_recent_tabs_sync_promo_instructions,
                new ButtonAbsent(), new ButtonAbsent());
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mSigninManager.addSignInStateObserver(this);
        AndroidSyncSettings.registerObserver(getContext(), this);
        update();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mSigninManager.removeSignInStateObserver(this);
        AndroidSyncSettings.unregisterObserver(getContext(), this);
    }

    // SigninStateObserver
    @Override
    public void onSignedIn() {
        update();
    }

    @Override
    public void onSignedOut() {
        update();
    }

    // AndroidSyncStateObserver
    @Override
    public void androidSyncSettingsChanged() {
        update();
    }
}