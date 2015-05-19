// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.Activity;
import android.app.FragmentTransaction;
import android.os.Bundle;
import android.preference.CheckBoxPreference;
import android.preference.Preference;
import android.preference.SwitchPreference;
import android.preference.TwoStatePreference;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.widget.ListView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.sync.ui.PassphraseTypeDialogFragment;
import org.chromium.chrome.browser.sync.ui.SyncCustomizationFragment;
import org.chromium.chrome.shell.R;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.internal_api.pub.PassphraseType;
import org.chromium.sync.internal_api.pub.base.ModelType;

import java.util.Collection;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * Tests for SyncCustomizationFragment.
 */
public class SyncCustomizationFragmentTest extends SyncTestBase {
    private static final String TAG = "SyncCustomizationFragmentTest";
    private static final String TEST_ACCOUNT = "test@gmail.com";

    /**
     * Maps ModelTypes to their UI element IDs.
     */
    private static final Map<ModelType, String> UI_DATATYPES;

    static {
        UI_DATATYPES = new HashMap<ModelType, String>();
        UI_DATATYPES.put(ModelType.AUTOFILL, SyncCustomizationFragment.PREFERENCE_SYNC_AUTOFILL);
        UI_DATATYPES.put(ModelType.BOOKMARK, SyncCustomizationFragment.PREFERENCE_SYNC_BOOKMARKS);
        UI_DATATYPES.put(ModelType.TYPED_URL, SyncCustomizationFragment.PREFERENCE_SYNC_OMNIBOX);
        UI_DATATYPES.put(ModelType.PASSWORD, SyncCustomizationFragment.PREFERENCE_SYNC_PASSWORDS);
        UI_DATATYPES.put(ModelType.PROXY_TABS,
                SyncCustomizationFragment.PREFERENCE_SYNC_RECENT_TABS);
    }

    private Activity mActivity;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
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
        assertTrue(AndroidSyncSettings.isChromeSyncEnabled(mContext));
        togglePreference(syncSwitch);
        assertFalse(syncSwitch.isChecked());
        assertFalse(AndroidSyncSettings.isChromeSyncEnabled(mContext));
        togglePreference(syncSwitch);
        assertTrue(syncSwitch.isChecked());
        assertTrue(AndroidSyncSettings.isChromeSyncEnabled(mContext));
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
        assertFalse(AndroidSyncSettings.isChromeSyncEnabled(mContext));
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
        Collection<CheckBoxPreference> dataTypes = getDataTypes(fragment).values();

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
        assertFalse(AndroidSyncSettings.isChromeSyncEnabled(mContext));
    }

    /**
     * Make sure that the encryption UI presents the correct options.
     *
     * By default it should show the CUSTOM and KEYSTORE options, in that order.
     * KEYSTORE should be selected but both should be enabled.
     */
    @SmallTest
    @Feature({"Sync"})
    public void testSettingDataTypes() throws Exception {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        startSync();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        SwitchPreference syncEverything = getSyncEverything(fragment);
        Map<ModelType, CheckBoxPreference> dataTypes = getDataTypes(fragment);

        assertDefaultSyncOnState(fragment);
        togglePreference(syncEverything);
        for (CheckBoxPreference dataType : dataTypes.values()) {
            assertTrue(dataType.isChecked());
            assertTrue(dataType.isEnabled());
        }

        Set<ModelType> expectedTypes = EnumSet.copyOf(dataTypes.keySet());
        assertDataTypesAre(expectedTypes);
        togglePreference(dataTypes.get(ModelType.AUTOFILL));
        togglePreference(dataTypes.get(ModelType.PASSWORD));
        // Nothing should have changed before the fragment closes.
        assertDataTypesAre(expectedTypes);

        closeFragment(fragment);
        expectedTypes.remove(ModelType.AUTOFILL);
        expectedTypes.remove(ModelType.PASSWORD);
        assertDataTypesAre(expectedTypes);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testDefaultEncryptionOptions() throws Exception {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        startSync();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        Preference encryption = getEncryption(fragment);
        clickPreference(encryption);

        PassphraseTypeDialogFragment typeFragment = getPassphraseTypeDialogFragment();
        ListView listView = (ListView) typeFragment.getDialog()
                .findViewById(R.id.passphrase_type_list);
        PassphraseTypeDialogFragment.Adapter adapter =
                (PassphraseTypeDialogFragment.Adapter) listView.getAdapter();

        // Confirm that correct types show up in the correct order.
        assertEquals(PassphraseType.CUSTOM_PASSPHRASE, adapter.getType(0));
        assertEquals(PassphraseType.KEYSTORE_PASSPHRASE, adapter.getType(1));
        assertEquals(2, listView.getCount());
        // Make sure they are both enabled and the correct on is selected.
        View customView = listView.getChildAt(0);
        View keystoreView = listView.getChildAt(1);
        assertTrue(customView.isEnabled());
        assertTrue(keystoreView.isEnabled());
        assertEquals(keystoreView, listView.getSelectedView());
    }

    private SyncCustomizationFragment startSyncCustomizationFragment() {
        SyncCustomizationFragment fragment = new SyncCustomizationFragment();
        Bundle args = new Bundle();
        args.putString(SyncCustomizationFragment.ARGUMENT_ACCOUNT,
                SyncTestUtil.DEFAULT_TEST_ACCOUNT);
        fragment.setArguments(args);
        FragmentTransaction transaction = mActivity.getFragmentManager().beginTransaction();
        transaction.add(R.id.content_container, fragment, TAG);
        transaction.commit();
        getInstrumentation().waitForIdleSync();
        return fragment;
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

    private Map<ModelType, CheckBoxPreference> getDataTypes(SyncCustomizationFragment fragment) {
        Map<ModelType, CheckBoxPreference> dataTypes =
                new HashMap<ModelType, CheckBoxPreference>();
        for (ModelType modelType : UI_DATATYPES.keySet()) {
            String prefId = UI_DATATYPES.get(modelType);
            dataTypes.put(modelType, (CheckBoxPreference) fragment.findPreference(prefId));
        }
        return dataTypes;
    }

    private Preference getEncryption(SyncCustomizationFragment fragment) {
        return (Preference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_ENCRYPTION);
    }

    private Preference getManageData(SyncCustomizationFragment fragment) {
        return (Preference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_SYNC_MANAGE_DATA);
    }

    private PassphraseTypeDialogFragment getPassphraseTypeDialogFragment() {
        return (PassphraseTypeDialogFragment) mActivity.getFragmentManager()
                .findFragmentByTag(SyncCustomizationFragment.FRAGMENT_PASSWORD_TYPE);
    }

    private void assertDefaultSyncOnState(SyncCustomizationFragment fragment) {
        assertTrue("The sync switch should be on.", getSyncSwitch(fragment).isChecked());
        assertTrue("The sync switch should be enabled.", getSyncSwitch(fragment).isEnabled());
        SwitchPreference syncEverything = getSyncEverything(fragment);
        assertTrue("The sync everything switch should be on.", syncEverything.isChecked());
        assertTrue("The sync everything switch should be enabled.", syncEverything.isEnabled());
        for (CheckBoxPreference dataType : getDataTypes(fragment).values()) {
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
        for (CheckBoxPreference dataType : getDataTypes(fragment).values()) {
            String key = dataType.getKey();
            assertTrue("Data type " + key + " should be checked.", dataType.isChecked());
            assertFalse("Data type " + key + " should be disabled.", dataType.isEnabled());
        }
        assertFalse("The encryption button should be disabled.",
                getEncryption(fragment).isEnabled());
        assertTrue("The manage sync data button should be always enabled.",
                getManageData(fragment).isEnabled());
    }

    private void assertDataTypesAre(final Set<ModelType> enabledDataTypes) {
        final Set<ModelType> disabledDataTypes = EnumSet.copyOf(UI_DATATYPES.keySet());
        disabledDataTypes.removeAll(enabledDataTypes);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Set<ModelType> actualDataTypes = mProfileSyncService.getPreferredDataTypes();
                assertTrue(actualDataTypes.containsAll(enabledDataTypes));
                // There is no Set.containsNone(), sadly.
                for (ModelType disabledDataType : disabledDataTypes) {
                    assertFalse(actualDataTypes.contains(disabledDataType));
                }
            }
        });
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

    private void clickPreference(final Preference pref) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                pref.getOnPreferenceClickListener().onPreferenceClick(pref);
            }
        });
        getInstrumentation().waitForIdleSync();
    }
}
