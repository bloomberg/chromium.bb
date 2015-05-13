// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.Activity;
import android.app.FragmentTransaction;
import android.preference.CheckBoxPreference;
import android.preference.Preference;
import android.preference.SwitchPreference;
import android.preference.TwoStatePreference;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.sync.ui.SyncCustomizationFragment;
import org.chromium.chrome.shell.R;
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
        startSync();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        final SwitchPreference syncSwitch = getSyncSwitch(fragment);

        assertTrue(syncSwitch.isChecked());
        assertTrue(mAndroidSyncSettings.isChromeSyncEnabled());
        togglePreference(syncSwitch);
        assertFalse(syncSwitch.isChecked());
        assertFalse(mAndroidSyncSettings.isChromeSyncEnabled());
        togglePreference(syncSwitch);
        assertTrue(syncSwitch.isChecked());
        assertTrue(mAndroidSyncSettings.isChromeSyncEnabled());
    }

    /**
     * This is a regression test for http://crbug.com/454939.
     */
    @SmallTest
    @Feature({"Sync"})
    public void testOpeningSettingsDoesntEnableSync() throws Exception {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        stopSync();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        closeFragment(fragment);
        assertFalse(mAndroidSyncSettings.isChromeSyncEnabled());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testDefaultControlStatesWithSyncOffThenOn() throws Exception {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        stopSync();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOffState(fragment);
        togglePreference(getSyncSwitch(fragment));
        waitForSyncInitialized();
        assertDefaultSyncOnState(fragment);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testDefaultControlStatesWithSyncOnThenOff() throws Exception {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        startSync();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        togglePreference(getSyncSwitch(fragment));
        assertDefaultSyncOffState(fragment);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testSyncEverythingAndDataTypes() throws Exception {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        startSync();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        SwitchPreference syncEverything = getSyncEverything(fragment);
        CheckBoxPreference[] dataTypes = getDataTypes(fragment);

        assertDefaultSyncOnState(fragment);
        togglePreference(syncEverything);
        for (CheckBoxPreference dataType : dataTypes) {
            assertTrue(dataType.isChecked());
            assertTrue(dataType.isEnabled());
        }

        // If all data types are unchecked, sync should turn off.
        for (CheckBoxPreference dataType : dataTypes) {
            togglePreference(dataType);
        }
        getInstrumentation().waitForIdleSync();
        assertDefaultSyncOffState(fragment);
        assertFalse(mAndroidSyncSettings.isChromeSyncEnabled());
    }

    private SyncCustomizationFragment startSyncCustomizationFragment() {
        FragmentTransaction transaction = mActivity.getFragmentManager().beginTransaction();
        transaction.add(R.id.content_container, new SyncCustomizationFragment(), TAG);
        transaction.commit();
        getInstrumentation().waitForIdleSync();
        return (SyncCustomizationFragment) mActivity.getFragmentManager().findFragmentByTag(TAG);
    }

    private void closeFragment(SyncCustomizationFragment fragment) {
        FragmentTransaction transaction = mActivity.getFragmentManager().beginTransaction();
        transaction.remove(fragment);
        transaction.commit();
        getInstrumentation().waitForIdleSync();
    }

    private SwitchPreference getSyncSwitch(SyncCustomizationFragment fragment) {
        return (SwitchPreference) fragment.findPreference(
                SyncCustomizationFragment.PREF_SYNC_SWITCH);
    }

    private SwitchPreference getSyncEverything(SyncCustomizationFragment fragment) {
        return (SwitchPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_SYNC_EVERYTHING);
    }

    private CheckBoxPreference[] getDataTypes(SyncCustomizationFragment fragment) {
        return new CheckBoxPreference[] {
            (CheckBoxPreference) fragment.findPreference(
                    SyncCustomizationFragment.PREFERENCE_SYNC_AUTOFILL),
            (CheckBoxPreference) fragment.findPreference(
                    SyncCustomizationFragment.PREFERENCE_SYNC_BOOKMARKS),
            (CheckBoxPreference) fragment.findPreference(
                    SyncCustomizationFragment.PREFERENCE_SYNC_OMNIBOX),
            (CheckBoxPreference) fragment.findPreference(
                    SyncCustomizationFragment.PREFERENCE_SYNC_PASSWORDS),
            (CheckBoxPreference) fragment.findPreference(
                    SyncCustomizationFragment.PREFERENCE_SYNC_RECENT_TABS)
        };
    }

    private Preference getEncryption(SyncCustomizationFragment fragment) {
        return (Preference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_ENCRYPTION);
    }

    private Preference getManageData(SyncCustomizationFragment fragment) {
        return (Preference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_SYNC_MANAGE_DATA);
    }

    private void assertDefaultSyncOnState(SyncCustomizationFragment fragment) {
        assertTrue("The sync switch should be on.", getSyncSwitch(fragment).isChecked());
        assertTrue("The sync switch should be enabled.", getSyncSwitch(fragment).isEnabled());
        SwitchPreference syncEverything = getSyncEverything(fragment);
        assertTrue("The sync everything switch should be on.", syncEverything.isChecked());
        assertTrue("The sync everything switch should be enabled.", syncEverything.isEnabled());
        for (CheckBoxPreference dataType : getDataTypes(fragment)) {
            String key = dataType.getKey();
            assertTrue("Data type " + key + " should be checked.", dataType.isChecked());
            assertFalse("Data type " + key + " should be disabled.", dataType.isEnabled());
        }
        assertTrue("The encryption button should be enabled.",
                getEncryption(fragment).isEnabled());
        assertTrue("The manage sync data button should be always enabled.",
                getManageData(fragment).isEnabled());
    }

    private void assertDefaultSyncOffState(SyncCustomizationFragment fragment) {
        assertFalse("The sync switch should be off.", getSyncSwitch(fragment).isChecked());
        assertTrue("The sync switch should be enabled.", getSyncSwitch(fragment).isEnabled());
        SwitchPreference syncEverything = getSyncEverything(fragment);
        assertTrue("The sync everything switch should be on.", syncEverything.isChecked());
        assertFalse("The sync everything switch should be disabled.", syncEverything.isEnabled());
        for (CheckBoxPreference dataType : getDataTypes(fragment)) {
            String key = dataType.getKey();
            assertTrue("Data type " + key + " should be checked.", dataType.isChecked());
            assertFalse("Data type " + key + " should be disabled.", dataType.isEnabled());
        }
        assertFalse("The encryption button should be disabled.",
                getEncryption(fragment).isEnabled());
        assertTrue("The manage sync data button should be always enabled.",
                getManageData(fragment).isEnabled());
    }

    private void togglePreference(final TwoStatePreference pref) {
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
