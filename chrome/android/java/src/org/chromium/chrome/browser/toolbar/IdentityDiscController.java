// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.annotation.DimenRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.SyncAndServicesPreferences;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.AndroidSyncSettings;

import java.util.Collections;

/**
 * Handles displaying IdentityDisc on toolbar depending on several conditions
 * (user sign-in state, whether NTP is shown)
 */
class IdentityDiscController implements ProfileDataCache.Observer {
    private final Context mContext;
    private final ToolbarManager mToolbarManager;

    private ProfileDataCache mProfileDataCache;
    private boolean mIsIdentityDiscVisible;

    /**
     * Creates IdentityDiscController object.
     * @param context The Context for retrieving resources, launching preference activiy, etc.
     * @param toolbarManager The ToolbarManager where Identity Disc is displayed.
     */
    IdentityDiscController(Context context, ToolbarManager toolbarManager) {
        mContext = context;
        mToolbarManager = toolbarManager;
    }

    /**
     * Shows/hides Identity Disc depending on whether NTP is visible.
     */
    void updateButtonState(boolean isNTPVisible) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.IDENTITY_DISC)) return;

        if (isNTPVisible && !mIsIdentityDiscVisible) {
            String accountName = ChromeSigninController.get().getSignedInAccountName();
            if (accountName != null && AndroidSyncSettings.get().isChromeSyncEnabled()) {
                createProfileDataCache();
                showIdentityDisc(accountName);
                mIsIdentityDiscVisible = true;
            }
        } else if (!isNTPVisible && mIsIdentityDiscVisible) {
            mIsIdentityDiscVisible = false;
            mToolbarManager.disableExperimentalButton();
        }
    }

    /**
     * Creates and initializes ProfileDataCache if it wasn't created previously. Subscribes
     * IdentityDiscController for profile data updates.
     */
    private void createProfileDataCache() {
        if (mProfileDataCache != null) return;

        @DimenRes
        int dimension_id =
                mToolbarManager.isBottomToolbarVisible() ? R.dimen.toolbar_identity_disc_size_duet
                                                         : R.dimen.toolbar_identity_disc_size;
        int imageSize = mContext.getResources().getDimensionPixelSize(dimension_id);
        mProfileDataCache = new ProfileDataCache(mContext, imageSize);
        mProfileDataCache.addObserver(this);
    }

    /**
     * Triggers profile image fetch and displays Identity Disc on top toolbar.
     */
    private void showIdentityDisc(String accountName) {
        mProfileDataCache.update(Collections.singletonList(accountName));
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

    /**
     * Call to tear down dependencies.
     */
    void destroy() {
        if (mProfileDataCache != null) {
            mProfileDataCache.removeObserver(this);
            mProfileDataCache = null;
        }
    }
}
