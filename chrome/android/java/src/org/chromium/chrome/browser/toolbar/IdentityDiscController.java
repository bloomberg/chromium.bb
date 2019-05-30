// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.annotation.DimenRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.SyncAndServicesPreferences;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.AndroidSyncSettings;

import java.util.Collections;

/**
 * Handles displaying IdentityDisc on toolbar depending on several conditions
 * (user sign-in state, whether NTP is shown)
 */
class IdentityDiscController implements NativeInitObserver, ProfileDataCache.Observer,
                                        SigninManager.SignInStateObserver,
                                        AndroidSyncSettings.AndroidSyncSettingsObserver {
    // Context is used for fetching resources and launching preferences page.
    private final Context mContext;
    // Toolbar manager exposes APIs for manipulating experimental button.
    private final ToolbarManager mToolbarManager;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    // SigninManager and AndroidSyncSettings allow observing sign-in and sync state.
    private SigninManager mSigninManager;
    private AndroidSyncSettings mAndroidSyncSettings;

    // ProfileDataCache facilitates retrieving profile picture.
    private ProfileDataCache mProfileDataCache;

    private boolean mIsIdentityDiscVisible;
    private boolean mIsNTPVisible;

    /**
     * Creates IdentityDiscController object.
     * @param context The Context for retrieving resources, launching preference activiy, etc.
     * @param toolbarManager The ToolbarManager where Identity Disc is displayed.
     */
    IdentityDiscController(Context context, ToolbarManager toolbarManager,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mContext = context;
        mToolbarManager = toolbarManager;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
    }

    /**
     * Registers itself to observe sign-in and sync status events.
     */
    @Override
    public void onFinishNativeInitialization() {
        mSigninManager = SigninManager.get();
        mSigninManager.addSignInStateObserver(this);

        mAndroidSyncSettings = AndroidSyncSettings.get();
        mAndroidSyncSettings.registerObserver(this);

        mActivityLifecycleDispatcher.unregister(this);
        mActivityLifecycleDispatcher = null;
    }

    /**
     * Shows/hides Identity Disc depending on whether NTP is visible.
     */
    void updateButtonState(boolean isNTPVisible) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.IDENTITY_DISC)) return;

        mIsNTPVisible = isNTPVisible;
        String accountName = ChromeSigninController.get().getSignedInAccountName();
        boolean shouldShowIdentityDisc =
                isNTPVisible && accountName != null && AndroidSyncSettings.get().isSyncEnabled();

        if (shouldShowIdentityDisc == mIsIdentityDiscVisible) return;

        if (shouldShowIdentityDisc) {
            mIsIdentityDiscVisible = true;
            createProfileDataCache(accountName);
            showIdentityDisc(accountName);
        } else {
            mIsIdentityDiscVisible = false;
            mToolbarManager.disableExperimentalButton();
        }
    }

    /**
     * Creates and initializes ProfileDataCache if it wasn't created previously. Subscribes
     * IdentityDiscController for profile data updates.
     */
    private void createProfileDataCache(String accountName) {
        if (mProfileDataCache != null) return;

        @DimenRes
        int dimension_id =
                mToolbarManager.isBottomToolbarVisible() ? R.dimen.toolbar_identity_disc_size_duet
                                                         : R.dimen.toolbar_identity_disc_size;
        int imageSize = mContext.getResources().getDimensionPixelSize(dimension_id);
        mProfileDataCache = new ProfileDataCache(mContext, imageSize);
        mProfileDataCache.addObserver(this);
        mProfileDataCache.update(Collections.singletonList(accountName));
    }

    /**
     * Triggers profile image fetch and displays Identity Disc on top toolbar.
     */
    private void showIdentityDisc(String accountName) {
        Drawable profileImage = mProfileDataCache.getProfileDataOrDefault(accountName).getImage();
        mToolbarManager.enableExperimentalButton(view -> {
            PreferencesLauncher.launchSettingsPage(mContext, SyncAndServicesPreferences.class);
        }, profileImage, R.string.accessibility_toolbar_btn_identity_disc);
    }

    /**
     * Called after profile image becomes available. Updates the image on toolbar button.
     */
    @Override
    public void onProfileDataUpdated(String accountId) {
        assert mProfileDataCache != null;
        if (!mIsIdentityDiscVisible) return;

        String accountName = ChromeSigninController.get().getSignedInAccountName();
        if (accountId.equals(accountName)) {
            Drawable image = mProfileDataCache.getProfileDataOrDefault(accountName).getImage();
            mToolbarManager.updateExperimentalButtonImage(image);
        }
    }

    // SigninManager.SignInStateObserver implementation.
    @Override
    public void onSignedIn() {
        String accountName = ChromeSigninController.get().getSignedInAccountName();
        if (mProfileDataCache != null && accountName != null) {
            mProfileDataCache.update(Collections.singletonList(accountName));
        }
        updateButtonState(mIsNTPVisible);
    }

    @Override
    public void onSignedOut() {
        updateButtonState(mIsNTPVisible);
    }

    // AndroidSyncSettings.AndroidSyncSettingsObserver implementation.
    @Override
    public void androidSyncSettingsChanged() {
        updateButtonState(mIsNTPVisible);
    }

    /**
     * Call to tear down dependencies.
     */
    void destroy() {
        if (mProfileDataCache != null) {
            mProfileDataCache.removeObserver(this);
            mProfileDataCache = null;
        }
        if (mSigninManager != null) {
            mSigninManager.removeSignInStateObserver(this);
            mSigninManager = null;
        }
        if (mAndroidSyncSettings != null) {
            mAndroidSyncSettings.unregisterObserver(this);
            mAndroidSyncSettings = null;
        }
    }
}
