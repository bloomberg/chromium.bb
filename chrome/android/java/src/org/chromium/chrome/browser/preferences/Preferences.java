// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.annotation.SuppressLint;
import android.app.Fragment;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources;
import android.graphics.BitmapFactory;
import android.nfc.NfcAdapter;
import android.os.Build;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceFragment.OnPreferenceStartFragmentCallback;
import android.support.v4.view.ViewCompat;
import android.support.v7.app.ActionBarActivity;
import android.util.Log;
import android.view.MenuItem;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.R;

/**
 * The Chrome settings activity.
 *
 * This activity displays a single Fragment, typically a PreferenceFragment. As the user navigates
 * through settings, a separate Preferences activity is created for each screen. Thus each fragment
 * may freely modify its activity's action bar or title. This mimics the behavior of
 * android.preference.PreferenceActivity.
 */
public abstract class Preferences extends ActionBarActivity implements
        OnPreferenceStartFragmentCallback {

    public static final String EXTRA_SHOW_FRAGMENT = "show_fragment";
    public static final String EXTRA_SHOW_FRAGMENT_ARGUMENTS = "show_fragment_args";
    public static final String EXTRA_DISPLAY_HOME_AS_UP = "display_home_as_up";

    private static final String TAG = "Preferences";

    /** The current instance of Preferences in the resumed state, if any. */
    private static Preferences sResumedInstance;

    /** Whether this activity has been created for the first time but not yet resumed. */
    private boolean mIsNewlyCreated;

    private static boolean sActivityNotExportedChecked;

    /**
     * Starts the browser process, if it's not already started.
     */
    protected abstract void startBrowserProcessSync() throws ProcessInitException;

    /**
     * Returns the name of the fragment to show if the intent doesn't request a specific fragment.
     */
    protected abstract String getTopLevelFragmentName();

    /**
     * Opens a URL in a new activity.
     * @param titleResId The resource ID of the title to show above the web page.
     * @param urlResId The resource ID of the URL to load.
     *
     * TODO(newt): remove this method when EmbedContentViewActivity is upstreamed.
     */
    public abstract void showUrl(int titleResId, int urlResId);

    /**
     * Launches the help page for Google translate.
     */
    public void showGoogleTranslateHelp() {}

    /**
     * Launches the help page for privacy settings.
     */
    public void showPrivacyPreferencesHelp() {}

    /**
     * Called when user changes the contextual search preference.
     * @param newValue Whether contextual search is now enabled.
     *
     * TODO(newt): remove this method when contextual search is upstreamed.
     */
    public void logContextualSearchToggled(boolean newValue) {}

    /**
     * Returns whether contextual search is enabled.
     *
     * TODO(newt): remove this method when contextual search is upstreamed.
     */
    public boolean isContextualSearchEnabled() {
        return false;
    }

    /**
     * Notifies the precache launcher that the user has changed the network prediction preference.
     *
     * TODO(newt): remove this method when precache logic is upstreamed.
     */
    public void updatePrecachingEnabled() {}

    @SuppressFBWarnings("DM_EXIT")
    @SuppressLint("InlinedApi")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        ensureActivityNotExported();

        // The browser process must be started here because this Activity may be started explicitly
        // from Android notifications, when Android is restoring Preferences after Chrome was
        // killed, or for tests. This should happen before super.onCreate() because it might
        // recreate a fragment, and a fragment might depend on the native library.
        try {
            startBrowserProcessSync();
        } catch (ProcessInitException e) {
            Log.e(TAG, "Failed to start browser process.", e);
            // This can only ever happen, if at all, when the activity is started from an Android
            // notification (or in tests). As such we don't want to show an error messsage to the
            // user. The application is completely broken at this point, so close it down
            // completely (not just the activity).
            System.exit(-1);
            return;
        }

        super.onCreate(savedInstanceState);

        mIsNewlyCreated = savedInstanceState == null;

        String initialFragment = getIntent().getStringExtra(EXTRA_SHOW_FRAGMENT);
        Bundle initialArguments = getIntent().getBundleExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS);
        boolean displayHomeAsUp = getIntent().getBooleanExtra(EXTRA_DISPLAY_HOME_AS_UP, true);

        if (displayHomeAsUp) getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        // This must be called before the fragment transaction below.
        workAroundPlatformBugs();

        // If savedInstanceState is non-null, then the activity is being
        // recreated and super.onCreate() has already recreated the fragment.
        if (savedInstanceState == null) {
            if (initialFragment == null) initialFragment = getTopLevelFragmentName();
            Fragment fragment = Fragment.instantiate(this, initialFragment, initialArguments);
            getFragmentManager().beginTransaction()
                    .replace(android.R.id.content, fragment)
                    .commit();
        }

        // Disable Android Beam on JB and later devices.
        // In ICS it does nothing - i.e. we will send a Play Store link if NFC is used.
        NfcAdapter nfcAdapter = NfcAdapter.getDefaultAdapter(this);
        if (nfcAdapter != null) nfcAdapter.setNdefPushMessage(null, this);

        Resources res = getResources();
        ApiCompatibilityUtils.setTaskDescription(this, res.getString(R.string.app_name),
                BitmapFactory.decodeResource(res, R.mipmap.app_icon),
                res.getColor(R.color.default_primary_color));
    }

    // OnPreferenceStartFragmentCallback:

    @Override
    public boolean onPreferenceStartFragment(PreferenceFragment preferenceFragment,
            Preference preference) {
        startFragment(preference.getFragment(), preference.getExtras());
        return true;
    }

    /**
     * Starts a new Preferences activity showing the desired fragment.
     *
     * @param fragmentClass The Class of the fragment to show.
     * @param args Arguments to pass to Fragment.instantiate(), or null.
     */
    public void startFragment(String fragmentClass, Bundle args) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClass(this, getClass());
        intent.putExtra(EXTRA_SHOW_FRAGMENT, fragmentClass);
        intent.putExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS, args);
        startActivity(intent);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            Fragment fragment = getFragmentManager().findFragmentById(android.R.id.content);
            if (fragment instanceof PreferenceFragment && fragment.getView() != null) {
                // Set list view padding to 0 so dividers are the full width of the screen.
                fragment.getView().findViewById(android.R.id.list).setPadding(0, 0, 0, 0);
            }
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        // Prevent the user from interacting with multiple instances of Preferences at the same time
        // (e.g. in multi-instance mode on a Samsung device), which would cause many fun bugs.
        if (sResumedInstance != null && !mIsNewlyCreated) {
            // This activity was unpaused or recreated while another instance of Preferences was
            // already showing. The existing instance takes precedence.
            finish();
        } else {
            // This activity was newly created and takes precedence over sResumedInstance.
            if (sResumedInstance != null) sResumedInstance.finish();
            sResumedInstance = this;
            mIsNewlyCreated = false;
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (sResumedInstance == this) sResumedInstance = null;
    }

    /**
     * Returns the fragment showing as this activity's main content, typically a PreferenceFragment.
     * This does not include DialogFragments or other Fragments shown on top of the main content.
     */
    @VisibleForTesting
    public Fragment getFragmentForTest() {
        return getFragmentManager().findFragmentById(android.R.id.content);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void ensureActivityNotExported() {
        if (sActivityNotExportedChecked) return;
        sActivityNotExportedChecked = true;
        try {
            ActivityInfo activityInfo = getPackageManager().getActivityInfo(getComponentName(), 0);
            // If Preferences is exported, then it's vulnerable to a fragment injection exploit:
            // http://securityintelligence.com/new-vulnerability-android-framework-fragment-injection
            if (activityInfo.exported) {
                throw new IllegalStateException("Preferences must not be exported.");
            }
        } catch (NameNotFoundException ex) {
            // Something terribly wrong has happened.
            throw new RuntimeException(ex);
        }
    }

    private void workAroundPlatformBugs() {
        // Workaround for an Android bug where the fragment's view may not be attached to the view
        // hierarchy. http://b/18525402
        getSupportActionBar();

        // Workaround for HTC One S bug which causes all the text in settings to turn white.
        // This must be called after setContentView().
        // https://code.google.com/p/android/issues/detail?id=78819
        ViewCompat.postOnAnimation(getWindow().getDecorView(), new Runnable() {
            @Override
            public void run() {
                setTheme(R.style.PreferencesTheme);
            }
        });
    }
}
