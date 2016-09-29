// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.settings;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.preference.SwitchPreference;
import android.support.v7.app.AlertDialog;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.R;
import org.chromium.blimp.core.common.PreferencesUtil;
import org.chromium.components.signin.ChromeSigninController;

/**
 * Blimp preferences page in embedder.
 */
@JNINamespace("blimp::client")
public class AboutBlimpPreferences extends PreferenceFragment {
    private static final String PREF_ENGINE_INFO = "blimp_engine_info";
    /**
     * If this fragment is waiting for user sign in.
     */
    @VisibleForTesting
    protected boolean mWaitForSignIn = false;

    private static BlimpPreferencesDelegate sPreferencesDelegate;

    private long mNativeBlimpSettingsAndroid;

    /**
     * Attach the blimp setting preferences to a {@link PreferenceFragment}.
     * And Set the delegate.
     * @param fragment The fragment that blimp setting attach to.
     * @param delegate {@link BlimpPreferencesDelegate} implemented by BlimpClientContextImpl.
     */
    public static void addBlimpPreferences(
            PreferenceFragment fragment, BlimpPreferencesDelegate delegate) {
        addBlimpPreferences(fragment);
        setDelegate(delegate);
    }

    private static void addBlimpPreferences(PreferenceFragment fragment) {
        PreferenceScreen screen = fragment.getPreferenceScreen();

        Preference blimpSetting = new Preference(fragment.getActivity());
        blimpSetting.setTitle(R.string.blimp_about_blimp_preferences);
        blimpSetting.setFragment(AboutBlimpPreferences.class.getName());
        blimpSetting.setKey(PreferencesUtil.PREF_BLIMP_SWITCH);

        screen.addPreference(blimpSetting);
        fragment.setPreferenceScreen(screen);
    }

    /**
     * Set {@link BlimpPreferencesDelegate}.
     */
    @VisibleForTesting
    protected static void setDelegate(BlimpPreferencesDelegate delegate) {
        sPreferencesDelegate = delegate;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActivity().setTitle(R.string.blimp_about_blimp_preferences);

        PreferenceScreen screen = getPreferenceScreen();
        if (screen != null) screen.removeAll();
        addPreferencesFromResource(R.xml.blimp_preferences);

        setupBlimpSwitch();
        setupAssignerPreferences();

        // Initialize native layer must be called after Java loads the prefrences entries from xml
        // file.
        initializeNative();
    }

    @Override
    public void onResume() {
        super.onResume();
        setupBlimpSwitch();
    }

    @Override
    public void onDestroy() {
        destroyNative();
        super.onDestroy();
    }

    /**
     * Setup the switch preference for Blimp.
     */
    private void setupBlimpSwitch() {
        // TODO(xingliu): Use {@link ChromeSwitchPreference} after move this class to Chrome.
        // http://crbug.com/630675
        final SwitchPreference pref =
                (SwitchPreference) findPreference(PreferencesUtil.PREF_BLIMP_SWITCH);

        if (!isSignedIn()) pref.setChecked(false);

        pref.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                return onBlimpSwitchPreferenceChange((boolean) newValue);
            }
        });
    }

    /**
      * Handles switch preference change.
      * @param switchValue The new value of the preference.
      * @return If the new value will be persisted.
      */
    private boolean onBlimpSwitchPreferenceChange(boolean switchValue) {
        if (switchValue) {
            if (isSignedIn()) {
                assert sPreferencesDelegate != null;

                // If user has signed in and the switch is turned on, start authentication.
                sPreferencesDelegate.connect();
            } else {
                // If user didn't sign in, show a dialog to let the user sign in.
                showSignInDialog();
                return false;
            }
        }
        return true;
    }

    /**
     * Show sign in dialog, it will show AccountSigninView to let user to sign in.
     *
     * If the user signed in after clicking the confirm button, turn on the Blimp switch and connect
     * to the engine.
     */
    private void showSignInDialog() {
        final Context context = getActivity();
        new AlertDialog.Builder(context)
                .setTitle(R.string.blimp_sign_in_title)
                .setMessage(R.string.blimp_sign_in_msg)
                .setPositiveButton(R.string.blimp_sign_in_btn,
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                startUserSignInFlow();
                            }
                        })
                .create()
                .show();
    }

    /**
     * When the user taps on the current assigner, a list of available assigners pops up.
     * User is allowed to change the assigner which is saved to shared preferences.
     * A dialog is displayed which prompts the user to restart the application.
     *
     * Use {@link PreferencesUtil#getLastUsedAssigner} to retrieve the assigner URL.
     */
    private void setupAssignerPreferences() {
        final Activity activity = getActivity();

        final ListPreference listPreference =
                (ListPreference) findPreference(PreferencesUtil.PREF_ASSIGNER_URL);

        // Set to default assigner URL on first time loading this UI.
        listPreference.setValue(PreferencesUtil.getLastUsedAssigner());

        listPreference.setSummary(listPreference.getValue());

        listPreference.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                String newAssignmentUrl = (String) newValue;
                listPreference.setSummary(newAssignmentUrl);
                showRestartDialog(activity, R.string.blimp_assigner_changed_please_restart);
                return true;
            }
        });
    }

    /**
     * Show restart browser dialog.
     * @param context The context where we display the restart browser dialog.
     * @param message The message shown to the user.
     */
    private void showRestartDialog(final Context context, int message) {
        new AlertDialog.Builder(context)
                .setTitle(R.string.blimp_restart_blimp)
                .setMessage(message)
                .setCancelable(false)
                .setPositiveButton(R.string.blimp_restart_now,
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                restartBrowser();
                            }
                        })
                .create()
                .show();
    }

    /**
     * Restart the browser.
     */
    @VisibleForTesting
    protected void restartBrowser() {
        assert sPreferencesDelegate != null;
        sPreferencesDelegate.getDelegate().restartBrowser();
    }

    /**
     * Start user sign in flow to let the user pick an existing account or create new account.
     */
    private void startUserSignInFlow() {
        mWaitForSignIn = true;
        assert sPreferencesDelegate != null;
        sPreferencesDelegate.getDelegate().startUserSignInFlow(getActivity());
    }

    private boolean isSignedIn() {
        return ChromeSigninController.get(ContextUtils.getApplicationContext()).isSignedIn();
    }

    @VisibleForTesting
    @CalledByNative
    protected void onSignedOut() {
        // If user signed out, turn off the switch. We also do a sign in state check in onResume.
        final SwitchPreference pref =
                (SwitchPreference) findPreference(PreferencesUtil.PREF_BLIMP_SWITCH);
        pref.setChecked(false);
        showRestartDialog(getActivity(), R.string.blimp_sign_out_restart);
    }

    @VisibleForTesting
    @CalledByNative
    protected void onSignedIn() {
        // If user came back from sign in flow, turn on the switch and connect to engine.
        // This logic won't trigger the {@link OnPreferenceChangeListener} call.
        if (mWaitForSignIn) {
            final SwitchPreference pref =
                    (SwitchPreference) findPreference(PreferencesUtil.PREF_BLIMP_SWITCH);
            pref.setChecked(true);

            assert sPreferencesDelegate != null;
            sPreferencesDelegate.connect();
            mWaitForSignIn = false;
        }
    }

    /**
     * Setup engine connection info. This entry is not persisted into shared preference.
     * @param engineInfo The engine IP address if connected to engine, or error string displayed to
     * the user.
     */
    @CalledByNative
    private void setEngineInfo(String engineInfo) {
        ThreadUtils.assertOnUiThread();

        Preference pref = findPreference(PREF_ENGINE_INFO);
        assert pref != null;
        pref.setSummary(engineInfo);
    }

    @VisibleForTesting
    protected void initializeNative() {
        mNativeBlimpSettingsAndroid = nativeInit();
        assert sPreferencesDelegate != null && mNativeBlimpSettingsAndroid != 0;

        // Initialize in native code.
        sPreferencesDelegate.initSettingsPage(this);
    }

    @VisibleForTesting
    protected void destroyNative() {
        nativeDestroy(mNativeBlimpSettingsAndroid);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeBlimpSettingsAndroid = 0;
    }

    @CalledByNative
    private long getNativePtr() {
        assert mNativeBlimpSettingsAndroid != 0;
        return mNativeBlimpSettingsAndroid;
    }

    private native long nativeInit();
    private native void nativeDestroy(long nativeBlimpSettingsAndroid);
}
