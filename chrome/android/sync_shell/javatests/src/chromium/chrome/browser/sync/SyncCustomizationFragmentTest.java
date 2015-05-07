// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.Activity;
import android.app.FragmentTransaction;
import android.preference.SwitchPreference;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.sync.ui.SyncCustomizationFragment;
import org.chromium.sync.AndroidSyncSettings;

/**
 * Tests for SyncCustomizationFragment.
 */
public class SyncCustomizationFragmentTest extends SyncTestBase {
    private static final String TAG = "SyncCustomizationFragmentTest";
    private static final String TEST_ACCOUNT = "test@gmail.com";

    private Activity mActivity;
    private AndroidSyncSettings mAndroidSyncSettings;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mAndroidSyncSettings = AndroidSyncSettings.get(mContext);
        mActivity = getActivity();
    }

    @SmallTest
    @Feature({"Sync"})
    public void testSyncSwitch() throws Exception {
        setupTestAccountAndSignInToSync(CLIENT_ID);

        // Make sure sync is actually enabled.
        mAndroidSyncSettings.enableChromeSync();
        getInstrumentation().waitForIdleSync();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        final SwitchPreference syncSwitch = getSyncSwitch(fragment);

        assertTrue(syncSwitch.isChecked());
        assertTrue(mAndroidSyncSettings.isChromeSyncEnabled());
        toggleSwitch(syncSwitch);
        assertFalse(syncSwitch.isChecked());
        assertFalse(mAndroidSyncSettings.isChromeSyncEnabled());
        toggleSwitch(syncSwitch);
        assertTrue(syncSwitch.isChecked());
        assertTrue(mAndroidSyncSettings.isChromeSyncEnabled());
    }

    private SyncCustomizationFragment startSyncCustomizationFragment() {
        FragmentTransaction transaction = mActivity.getFragmentManager().beginTransaction();
        transaction.add(R.id.content_container, new SyncCustomizationFragment(), TAG);
        transaction.commit();
        getInstrumentation().waitForIdleSync();
        return (SyncCustomizationFragment) mActivity.getFragmentManager().findFragmentByTag(TAG);
    }

    private SwitchPreference getSyncSwitch(SyncCustomizationFragment fragment) {
        return (SwitchPreference) fragment.findPreference(
                SyncCustomizationFragment.PREF_SYNC_SWITCH);
    }

    private SwitchPreference getSyncEverything(SyncCustomizationFragment fragment) {
        return (SwitchPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_SYNC_EVERYTHING);
    }

    private void toggleSwitch(final SwitchPreference pref) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                boolean newValue = !pref.isChecked();
                pref.getOnPreferenceChangeListener().onPreferenceChange(
                        pref, Boolean.valueOf(newValue));
                pref.setChecked(newValue);
            }
        });
        getInstrumentation().waitForIdleSync();
    }
}
