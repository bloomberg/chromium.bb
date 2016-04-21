// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.app.FragmentTransaction;
import android.preference.CheckBoxPreference;
import android.preference.Preference;
import android.preference.SwitchPreference;
import android.preference.TwoStatePreference;
import android.support.v7.app.AlertDialog;
import android.test.suitebuilder.annotation.SmallTest;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.sync.ui.PassphraseCreationDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseTypeDialogFragment;
import org.chromium.chrome.browser.sync.ui.SyncCustomizationFragment;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.ModelType;
import org.chromium.sync.PassphraseType;

import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Callable;

/**
 * Tests for SyncCustomizationFragment.
 */
@SuppressLint("UseSparseArrays")
public class SyncCustomizationFragmentTest extends SyncTestBase {
    private static final String TAG = "SyncCustomizationFragmentTest";

    /**
     * Fake ProfileSyncService for test to control the value returned from
     * isPassphraseRequiredForDecryption.
     */
    private class FakeProfileSyncService extends ProfileSyncService {
        private boolean mPassphraseRequiredForDecryption;

        public FakeProfileSyncService() {
            super();
            setMasterSyncEnabledProvider(new MasterSyncEnabledProvider() {
                @Override
                public boolean isMasterSyncEnabled() {
                    return AndroidSyncSettings.isMasterSyncEnabled(mContext);
                }
            });
        }

        @Override
        public boolean isPassphraseRequiredForDecryption() {
            return mPassphraseRequiredForDecryption;
        }

        public void setPassphraseRequiredForDecryption(boolean passphraseRequiredForDecryption) {
            mPassphraseRequiredForDecryption = passphraseRequiredForDecryption;
        }
    }

    /**
     * Maps ModelTypes to their UI element IDs.
     */
    private static final Map<Integer, String> UI_DATATYPES;

    static {
        UI_DATATYPES = new HashMap<Integer, String>();
        UI_DATATYPES.put(ModelType.AUTOFILL, SyncCustomizationFragment.PREFERENCE_SYNC_AUTOFILL);
        UI_DATATYPES.put(ModelType.BOOKMARKS, SyncCustomizationFragment.PREFERENCE_SYNC_BOOKMARKS);
        UI_DATATYPES.put(ModelType.TYPED_URLS, SyncCustomizationFragment.PREFERENCE_SYNC_OMNIBOX);
        UI_DATATYPES.put(ModelType.PASSWORDS, SyncCustomizationFragment.PREFERENCE_SYNC_PASSWORDS);
        UI_DATATYPES.put(ModelType.PROXY_TABS,
                SyncCustomizationFragment.PREFERENCE_SYNC_RECENT_TABS);
        UI_DATATYPES.put(ModelType.PREFERENCES,
                SyncCustomizationFragment.PREFERENCE_SYNC_SETTINGS);
    }

    private Preferences mPreferences;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mPreferences = null;
    }

    @SmallTest
    @Feature({"Sync"})
    public void testSyncSwitch() throws Exception {
        setUpTestAccountAndSignInToSync();
        SyncTestUtil.waitForSyncActive();
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
        setUpTestAccountAndSignInToSync();
        stopSync();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        closeFragment(fragment);
        assertFalse(AndroidSyncSettings.isChromeSyncEnabled(mContext));
    }

    /**
     * This is a regression test for http://crbug.com/467600.
     */
    @SmallTest
    @Feature({"Sync"})
    public void testOpeningSettingsDoesntStartBackend() throws Exception {
        setUpTestAccountAndSignInToSync();
        stopSync();
        startSyncCustomizationFragment();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertFalse(mProfileSyncService.isSyncRequested());
                assertFalse(mProfileSyncService.isBackendInitialized());
            }
        });
    }

    @SmallTest
    @Feature({"Sync"})
    public void testDefaultControlStatesWithSyncOffThenOn() throws Exception {
        setUpTestAccountAndSignInToSync();
        stopSync();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOffState(fragment);
        togglePreference(getSyncSwitch(fragment));
        waitForBackendInitialized();
        assertDefaultSyncOnState(fragment);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testDefaultControlStatesWithSyncOnThenOff() throws Exception {
        setUpTestAccountAndSignInToSync();
        SyncTestUtil.waitForSyncActive();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        togglePreference(getSyncSwitch(fragment));
        assertDefaultSyncOffState(fragment);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testSyncEverythingAndDataTypes() throws Exception {
        setUpTestAccountAndSignInToSync();
        SyncTestUtil.waitForSyncActive();
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

    @SmallTest
    @Feature({"Sync"})
    public void testSettingDataTypes() throws Exception {
        setUpTestAccountAndSignInToSync();
        SyncTestUtil.waitForSyncActive();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        SwitchPreference syncEverything = getSyncEverything(fragment);
        Map<Integer, CheckBoxPreference> dataTypes = getDataTypes(fragment);

        assertDefaultSyncOnState(fragment);
        togglePreference(syncEverything);
        for (CheckBoxPreference dataType : dataTypes.values()) {
            assertTrue(dataType.isChecked());
            assertTrue(dataType.isEnabled());
        }

        Set<Integer> expectedTypes = new HashSet<Integer>(dataTypes.keySet());
        // TODO(zea): update this once preferences are supported.
        expectedTypes.remove(ModelType.PREFERENCES);
        expectedTypes.add(ModelType.PRIORITY_PREFERENCES);
        assertDataTypesAre(expectedTypes);
        togglePreference(dataTypes.get(ModelType.AUTOFILL));
        togglePreference(dataTypes.get(ModelType.PASSWORDS));
        // Nothing should have changed before the fragment closes.
        assertDataTypesAre(expectedTypes);

        closeFragment(fragment);
        expectedTypes.remove(ModelType.AUTOFILL);
        expectedTypes.remove(ModelType.PASSWORDS);
        assertDataTypesAre(expectedTypes);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationChecked() throws Exception {
        setUpTestAccountAndSignInToSync();

        setPaymentsIntegrationEnabled(true);

        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_PAYMENTS_INTEGRATION);

        assertFalse(paymentsIntegration.isEnabled());
        assertTrue(paymentsIntegration.isChecked());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationUnchecked() throws Exception {
        setUpTestAccountAndSignInToSync();

        setPaymentsIntegrationEnabled(false);

        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        SwitchPreference syncEverything = getSyncEverything(fragment);
        togglePreference(syncEverything);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_PAYMENTS_INTEGRATION);

        assertTrue(paymentsIntegration.isEnabled());
        assertFalse(paymentsIntegration.isChecked());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationCheckboxDisablesPaymentsIntegration() throws Exception {
        setUpTestAccountAndSignInToSync();

        setPaymentsIntegrationEnabled(true);

        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        SwitchPreference syncEverything = getSyncEverything(fragment);
        togglePreference(syncEverything);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_PAYMENTS_INTEGRATION);
        togglePreference(paymentsIntegration);

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(false);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationCheckboxEnablesPaymentsIntegration() throws Exception {
        setUpTestAccountAndSignInToSync();

        setPaymentsIntegrationEnabled(false);

        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        SwitchPreference syncEverything = getSyncEverything(fragment);
        togglePreference(syncEverything);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_PAYMENTS_INTEGRATION);
        togglePreference(paymentsIntegration);

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(true);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationDisabledByAutofillSyncCheckbox() throws Exception {
        setUpTestAccountAndSignInToSync();

        setPaymentsIntegrationEnabled(true);

        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        SwitchPreference syncEverything = getSyncEverything(fragment);
        togglePreference(syncEverything);

        CheckBoxPreference syncAutofill = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_SYNC_AUTOFILL);
        togglePreference(syncAutofill);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_PAYMENTS_INTEGRATION);
        assertFalse(paymentsIntegration.isEnabled());
        assertFalse(paymentsIntegration.isChecked());

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(false);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationEnabledByAutofillSyncCheckbox() throws Exception {
        setUpTestAccountAndSignInToSync();

        setPaymentsIntegrationEnabled(false);

        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        SwitchPreference syncEverything = getSyncEverything(fragment);
        togglePreference(syncEverything);

        CheckBoxPreference syncAutofill = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_SYNC_AUTOFILL);
        togglePreference(syncAutofill);  // Disable autofill sync.
        togglePreference(syncAutofill);  // Re-enable autofill sync again.

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_PAYMENTS_INTEGRATION);
        assertTrue(paymentsIntegration.isEnabled());
        assertTrue(paymentsIntegration.isChecked());

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(true);
    }
    /**
     * Test that choosing a passphrase type while sync is off doesn't crash.
     *
     * This is a regression test for http://crbug.com/507557.
     */
    @SmallTest
    @Feature({"Sync"})
    public void testChoosePassphraseTypeWhenSyncIsOff() throws Exception {
        setUpTestAccountAndSignInToSync();
        SyncTestUtil.waitForSyncActive();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        Preference encryption = getEncryption(fragment);
        clickPreference(encryption);

        final PassphraseTypeDialogFragment typeFragment = getPassphraseTypeDialogFragment();
        stopSync();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                typeFragment.onItemClick(
                        null, null, 0, PassphraseType.CUSTOM_PASSPHRASE.internalValue());
            }
        });
        // No crash means we passed.
    }

    /**
     * Test that entering a passphrase while sync is off doesn't crash.
     */
    @SmallTest
    @Feature({"Sync"})
    public void testEnterPassphraseWhenSyncIsOff() throws Exception {
        setUpTestAccountAndSignInToSync();
        SyncTestUtil.waitForSyncActive();
        final SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        stopSync();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                fragment.onPassphraseEntered("passphrase");
            }
        });
        // No crash means we passed.
    }

    /**
     * Test that triggering OnPassphraseAccepted dismisses PassphraseDialogFragment.
     */
    @SmallTest
    @Feature({"Sync"})
    public void testPassphraseDialogDismissed() throws Exception {
        final FakeProfileSyncService pss = overrideProfileSyncService();

        setUpTestAccountAndSignInToSync();
        SyncTestUtil.waitForSyncActive();
        // Trigger PassphraseDialogFragment to be shown when taping on Encryption.
        pss.setPassphraseRequiredForDecryption(true);

        final SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        Preference encryption = getEncryption(fragment);
        clickPreference(encryption);

        final PassphraseDialogFragment passphraseFragment = getPassphraseDialogFragment();
        assertTrue(passphraseFragment.isAdded());

        // Simulate OnPassphraseAccepted from external event by setting PassphraseRequired to false
        // and triggering syncStateChanged().
        // PassphraseDialogFragment should be dismissed.
        pss.setPassphraseRequiredForDecryption(false);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                pss.syncStateChanged();
                fragment.getFragmentManager().executePendingTransactions();
                assertNull("PassphraseDialogFragment should be dismissed.",
                        mPreferences.getFragmentManager().findFragmentByTag(
                                SyncCustomizationFragment.FRAGMENT_ENTER_PASSPHRASE));
            }
        });
    }

    @SmallTest
    @Feature({"Sync"})
    public void testPassphraseCreation() throws Exception {
        setUpTestAccountAndSignInToSync();
        SyncTestUtil.waitForSyncActive();
        final SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                fragment.onPassphraseTypeSelected(PassphraseType.CUSTOM_PASSPHRASE);
            }
        });
        getInstrumentation().waitForIdleSync();
        PassphraseCreationDialogFragment pcdf = getPassphraseCreationDialogFragment();
        AlertDialog dialog = (AlertDialog) pcdf.getDialog();
        Button okButton = dialog.getButton(Dialog.BUTTON_POSITIVE);
        EditText enterPassphrase = (EditText) dialog.findViewById(R.id.passphrase);
        EditText confirmPassphrase = (EditText) dialog.findViewById(R.id.confirm_passphrase);

        // Error if you try to submit empty passphrase.
        assertNull(confirmPassphrase.getError());
        clickButton(okButton);
        assertTrue(pcdf.isResumed());
        assertNotNull(enterPassphrase.getError());

        // Error if you try to submit with only the first box filled.
        clearError(confirmPassphrase);
        setText(enterPassphrase, "foo");
        clickButton(okButton);
        assertTrue(pcdf.isResumed());
        assertNotNull(confirmPassphrase.getError());

        // Error if you try to submit with only the second box filled.
        setText(enterPassphrase, "");
        clearError(confirmPassphrase);
        setText(confirmPassphrase, "foo");
        clickButton(okButton);
        assertTrue(pcdf.isResumed());
        assertNotNull(confirmPassphrase.getError());

        // No error if text doesn't match without button press.
        setText(enterPassphrase, "foo");
        clearError(confirmPassphrase);
        setText(confirmPassphrase, "bar");
        assertNull(confirmPassphrase.getError());

        // Error if you try to submit unmatching text.
        clearError(confirmPassphrase);
        clickButton(okButton);
        assertTrue(pcdf.isResumed());
        assertNotNull(confirmPassphrase.getError());

        // Success if text matches.
        setText(confirmPassphrase, "foo");
        clickButton(okButton);
        assertFalse(pcdf.isResumed());
    }

    private FakeProfileSyncService overrideProfileSyncService() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<FakeProfileSyncService>() {
            @Override
            public FakeProfileSyncService call() {
                // PSS has to be constructed on the UI thread.
                FakeProfileSyncService fakeProfileSyncService = new FakeProfileSyncService();
                ProfileSyncService.overrideForTests(fakeProfileSyncService);
                return fakeProfileSyncService;
            }
        });
    }

    private SyncCustomizationFragment startSyncCustomizationFragment() {
        mPreferences = startPreferences(SyncCustomizationFragment.class.getName());
        getInstrumentation().waitForIdleSync();
        return (SyncCustomizationFragment) mPreferences.getFragmentForTest();
    }

    private void closeFragment(SyncCustomizationFragment fragment) {
        FragmentTransaction transaction = mPreferences.getFragmentManager().beginTransaction();
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

    private Map<Integer, CheckBoxPreference> getDataTypes(SyncCustomizationFragment fragment) {
        Map<Integer, CheckBoxPreference> dataTypes =
                new HashMap<Integer, CheckBoxPreference>();
        for (Map.Entry<Integer, String> uiDataType : UI_DATATYPES.entrySet()) {
            Integer modelType = uiDataType.getKey();
            String prefId = uiDataType.getValue();
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

    private PassphraseDialogFragment getPassphraseDialogFragment()
            throws InterruptedException {
        return ActivityUtils.<PassphraseDialogFragment>waitForFragment(mPreferences,
                SyncCustomizationFragment.FRAGMENT_ENTER_PASSPHRASE);
    }

    private PassphraseTypeDialogFragment getPassphraseTypeDialogFragment()
            throws InterruptedException {
        return ActivityUtils.<PassphraseTypeDialogFragment>waitForFragment(mPreferences,
                SyncCustomizationFragment.FRAGMENT_PASSPHRASE_TYPE);
    }

    private PassphraseCreationDialogFragment getPassphraseCreationDialogFragment()
            throws InterruptedException {
        return ActivityUtils.<PassphraseCreationDialogFragment>waitForFragment(mPreferences,
                SyncCustomizationFragment.FRAGMENT_CUSTOM_PASSPHRASE);
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

    private void assertDataTypesAre(final Set<Integer> enabledDataTypes) {
        final Set<Integer> disabledDataTypes = new HashSet<Integer>(UI_DATATYPES.keySet());
        disabledDataTypes.removeAll(enabledDataTypes);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Set<Integer> actualDataTypes = mProfileSyncService.getPreferredDataTypes();
                assertTrue(actualDataTypes.containsAll(enabledDataTypes));
                // There is no Set.containsNone(), sadly.
                for (Integer disabledDataType : disabledDataTypes) {
                    assertFalse(actualDataTypes.contains(disabledDataType));
                }
            }
        });
    }

    private void setPaymentsIntegrationEnabled(final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PersonalDataManager.setPaymentsIntegrationEnabled(enabled);
            }
        });
    }

    private void assertPaymentsIntegrationEnabled(final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertEquals(enabled, PersonalDataManager.isPaymentsIntegrationEnabled());
            }
        });
    }

    private void waitForBackendInitialized() throws InterruptedException {
        CriteriaHelper.pollUiThread(new Criteria(
                "Timed out waiting for sync's backend to be initialized.") {
            @Override
            public boolean isSatisfied() {
                return mProfileSyncService.isBackendInitialized();
            }
        }, SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }

    // UI interaction convenience methods.

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

    private void clickButton(final Button button) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                button.performClick();
            }
        });
        getInstrumentation().waitForIdleSync();
    }

    private void setText(final TextView textView, final String text) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                textView.setText(text);
            }
        });
        getInstrumentation().waitForIdleSync();
    }

    private void clearError(final TextView textView) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                textView.setError(null);
            }
        });
        getInstrumentation().waitForIdleSync();
    }
}
