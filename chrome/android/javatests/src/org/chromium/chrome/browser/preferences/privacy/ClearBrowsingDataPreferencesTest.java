// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.content.Intent;
import android.os.AsyncTask;
import android.preference.CheckBoxPreference;
import android.preference.Preference;
import android.preference.PreferenceScreen;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v7.app.AlertDialog;
import android.text.SpannableString;
import android.widget.ListView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.preferences.ButtonPreference;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.privacy.ClearBrowsingDataPreferences.DialogOption;
import org.chromium.chrome.browser.webapps.TestFetchStorageCallback;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;

import java.net.URL;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/**
 * Integration tests for ClearBrowsingDataPreferences.
 */
@RetryOnFailure
public class ClearBrowsingDataPreferencesTest
        extends ChromeActivityTestCaseBase<ChromeActivity> {
    private EmbeddedTestServer mTestServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    public ClearBrowsingDataPreferencesTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        SigninTestUtil.setUpAuthForTest(getInstrumentation());
        startMainActivityOnBlankPage();
    }

    /**  Waits for the progress dialog to disappear from the given CBD preference. */
    private void waitForProgressToComplete(final ClearBrowsingDataPreferences preferences)
            throws Exception {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return preferences.getProgressDialog() == null;
            }
        });
    }

    /**
     * Tests that web apps are cleared when the "cookies and site data" option is selected.
     */
    @MediumTest
    public void testClearingSiteDataClearsWebapps() throws Exception {
        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register("first", callback);
        callback.waitForCallback(0);
        assertEquals(new HashSet<String>(Arrays.asList("first")),
                WebappRegistry.getRegisteredWebappIdsForTesting());

        setDataTypesToClear(Arrays.asList(DialogOption.CLEAR_COOKIES_AND_SITE_DATA));
        final ClearBrowsingDataPreferences preferences =
                (ClearBrowsingDataPreferences) startPreferences(
                        ClearBrowsingDataPreferences.class.getName())
                        .getFragmentForTest();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PreferenceScreen screen = preferences.getPreferenceScreen();
                ButtonPreference clearButton = (ButtonPreference) screen.findPreference(
                        ClearBrowsingDataPreferences.PREF_CLEAR_BUTTON);
                clearButton.getOnPreferenceClickListener().onPreferenceClick(clearButton);
            }
        });
        waitForProgressToComplete(preferences);

        assertTrue(WebappRegistry.getRegisteredWebappIdsForTesting().isEmpty());
    }

    /**
     * Tests that web app scopes and last launch times are cleared when the "history" option is
     * selected. However, the web app is not removed from the registry.
     */
    @MediumTest
    public void testClearingHistoryClearsWebappScopesAndLaunchTimes() throws Exception {
        AsyncTask<Void, Void, Intent> shortcutIntentTask = new AsyncTask<Void, Void, Intent>() {
            @Override
            protected Intent doInBackground(Void... nothing) {
                return ShortcutHelper.createWebappShortcutIntentForTesting("id", "url");
            }
        };
        final Intent shortcutIntent = shortcutIntentTask.execute().get();

        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register("first", callback);
        callback.waitForCallback(0);
        callback.getStorage().updateFromShortcutIntent(shortcutIntent);

        assertEquals(new HashSet<String>(Arrays.asList("first")),
                WebappRegistry.getRegisteredWebappIdsForTesting());

        setDataTypesToClear(Arrays.asList(DialogOption.CLEAR_HISTORY));
        final ClearBrowsingDataPreferences preferences =
                (ClearBrowsingDataPreferences) startPreferences(
                        ClearBrowsingDataPreferences.class.getName())
                        .getFragmentForTest();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PreferenceScreen screen = preferences.getPreferenceScreen();
                ButtonPreference clearButton = (ButtonPreference) screen.findPreference(
                        ClearBrowsingDataPreferences.PREF_CLEAR_BUTTON);
                clearButton.getOnPreferenceClickListener().onPreferenceClick(clearButton);
            }
        });
        waitForProgressToComplete(preferences);

        assertEquals(new HashSet<String>(Arrays.asList("first")),
                WebappRegistry.getRegisteredWebappIdsForTesting());

        // URL and scope should be empty, and last used time should be 0.
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage("first");
        assertEquals("", storage.getScope());
        assertEquals("", storage.getUrl());
        assertEquals(0, storage.getLastUsedTime());
    }

    /**
     * Tests that a fragment with all options preselected indeed has all checkboxes checked
     * on startup, and that deletion with all checkboxes checked completes successfully.
     */
    @MediumTest
    public void testClearingEverything() throws Exception {
        setDataTypesToClear(Arrays.asList(DialogOption.values()));

        final ClearBrowsingDataPreferences preferences =
                (ClearBrowsingDataPreferences) startPreferences(
                        ClearBrowsingDataPreferences.class.getName())
                        .getFragmentForTest();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PreferenceScreen screen = preferences.getPreferenceScreen();

                for (int i = 0; i < screen.getPreferenceCount(); ++i) {
                    Preference pref = screen.getPreference(i);
                    if (!(pref instanceof CheckBoxPreference)) {
                        continue;
                    }
                    CheckBoxPreference checkbox = (CheckBoxPreference) pref;
                    assertTrue(checkbox.isChecked());
                }

                ButtonPreference clearButton = (ButtonPreference) screen.findPreference(
                        ClearBrowsingDataPreferences.PREF_CLEAR_BUTTON);
                assertTrue(clearButton.isEnabled());
                clearButton.getOnPreferenceClickListener().onPreferenceClick(clearButton);
            }
        });

        waitForProgressToComplete(preferences);
    }

    /**
     * Tests that for users who are not signed in, only the general footnote is shown.
     */
    @SmallTest
    public void testFooterNonsigned() throws Exception {
        SigninTestUtil.resetSigninState();

        final Preferences preferences =
                startPreferences(ClearBrowsingDataPreferences.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClearBrowsingDataPreferences fragment =
                        (ClearBrowsingDataPreferences) preferences.getFragmentForTest();
                PreferenceScreen screen = fragment.getPreferenceScreen();

                assertNotNull(
                        screen.findPreference(ClearBrowsingDataPreferences.PREF_GENERAL_SUMMARY));
                assertNull(
                        screen.findPreference(ClearBrowsingDataPreferences.PREF_GOOGLE_SUMMARY));
            }
        });
    }

    /**
     * Tests that for users who are signed in, both the general and the Google-specific footnotes
     * are shown.
     */
    @MediumTest
    public void testFooterSigned() throws Exception {
        // Sign in.
        SigninTestUtil.addAndSignInTestAccount();

        final Preferences preferences =
                startPreferences(ClearBrowsingDataPreferences.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClearBrowsingDataPreferences fragment =
                        (ClearBrowsingDataPreferences) preferences.getFragmentForTest();
                PreferenceScreen screen = fragment.getPreferenceScreen();

                assertNotNull(
                        screen.findPreference(ClearBrowsingDataPreferences.PREF_GENERAL_SUMMARY));

                Preference google_summary =
                        screen.findPreference(ClearBrowsingDataPreferences.PREF_GOOGLE_SUMMARY);
                assertNotNull(google_summary);

                // There is currently no clickable link in the Google-specific summary.
                assertTrue(!(google_summary.getSummary() instanceof SpannableString)
                        || ((SpannableString) google_summary.getSummary()).getSpans(
                                0, google_summary.getSummary().length(), Object.class).length == 0);

                // When the web history service reports that there are other forms of browsing
                // history, we should show a link to them.
                fragment.showNoticeAboutOtherFormsOfBrowsingHistory();
                assertTrue(google_summary.getSummary() instanceof SpannableString);
                assertTrue(((SpannableString) google_summary.getSummary()).getSpans(
                        0, google_summary.getSummary().length(), Object.class).length == 1);
            }
        });
    }

    /**
     * A helper Runnable that opens the Preferences activity containing
     * a ClearBrowsingDataPreferences fragment and clicks the "Clear" button.
     */
    static class OpenPreferencesEnableDialogAndClickClearRunnable implements Runnable {
        final Preferences mPreferences;

        /**
         * Instantiates this OpenPreferencesEnableDialogAndClickClearRunnable.
         * @param preferences A Preferences activity containing ClearBrowsingDataPreferences
         *         fragment.
         */
        public OpenPreferencesEnableDialogAndClickClearRunnable(Preferences preferences) {
            mPreferences = preferences;
        }

        @Override
        public void run() {
            ClearBrowsingDataPreferences fragment =
                    (ClearBrowsingDataPreferences) mPreferences.getFragmentForTest();
            PreferenceScreen screen = fragment.getPreferenceScreen();

            // Enable the dialog and click the "Clear" button.
            fragment.enableDialogAboutOtherFormsOfBrowsingHistory();
            ButtonPreference clearButton = (ButtonPreference) screen.findPreference(
                    ClearBrowsingDataPreferences.PREF_CLEAR_BUTTON);
            assertTrue(clearButton.isEnabled());
            clearButton.getOnPreferenceClickListener().onPreferenceClick(clearButton);
        }
    }

    /**
     * A criterion that is satisfied when a ClearBrowsingDataPreferences fragment in the given
     * Preferences activity is closed.
     */
    static class PreferenceScreenClosedCriterion extends Criteria {
        final Preferences mPreferences;

        /**
         * Instantiates this PreferenceScreenClosedCriterion.
         * @param preferences A Preferences activity containing ClearBrowsingDataPreferences
         *         fragment.
         */
        public PreferenceScreenClosedCriterion(Preferences preferences) {
            mPreferences = preferences;
        }

        @Override
        public boolean isSatisfied() {
            ClearBrowsingDataPreferences fragment =
                    (ClearBrowsingDataPreferences) mPreferences.getFragmentForTest();
            return fragment == null || !fragment.isVisible();
        }
    }

    /**
     * Tests that if the dialog about other forms of browsing history is enabled, it will be shown
     * after the deletion completes, if and only if browsing history was checked for deletion
     * and it has not been shown before.
     */
    @LargeTest
    public void testDialogAboutOtherFormsOfBrowsingHistory() throws Exception {
        // Sign in.
        SigninTestUtil.addAndSignInTestAccount();
        OtherFormsOfHistoryDialogFragment.clearShownPreferenceForTesting(getActivity());

        // History is not selected. We still need to select some other datatype, otherwise the
        // "Clear" button won't be enabled.
        setDataTypesToClear(Arrays.asList(DialogOption.CLEAR_CACHE));
        final Preferences preferences1 =
                startPreferences(ClearBrowsingDataPreferences.class.getName());
        ThreadUtils.runOnUiThreadBlocking(
                new OpenPreferencesEnableDialogAndClickClearRunnable(preferences1));

        // The dialog about other forms of history is not shown. The Clear Browsing Data preferences
        // is closed as usual.
        CriteriaHelper.pollUiThread(new PreferenceScreenClosedCriterion(preferences1));

        // Reopen Clear Browsing Data preferences, this time with history selected for clearing.
        setDataTypesToClear(Arrays.asList(DialogOption.CLEAR_HISTORY));
        final Preferences preferences2 =
                startPreferences(ClearBrowsingDataPreferences.class.getName());
        ThreadUtils.runOnUiThreadBlocking(
                new OpenPreferencesEnableDialogAndClickClearRunnable(preferences2));

        // The dialog about other forms of history should now be shown.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ClearBrowsingDataPreferences fragment =
                        (ClearBrowsingDataPreferences) preferences2.getFragmentForTest();
                OtherFormsOfHistoryDialogFragment dialog =
                        fragment.getDialogAboutOtherFormsOfBrowsingHistory();
                return dialog != null;
            }
        });

        // Close that dialog.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClearBrowsingDataPreferences fragment =
                        (ClearBrowsingDataPreferences) preferences2.getFragmentForTest();
                fragment.getDialogAboutOtherFormsOfBrowsingHistory().onClick(
                        null, AlertDialog.BUTTON_POSITIVE);
            }
        });

        // That should close the preference screen as well.
        CriteriaHelper.pollUiThread(new PreferenceScreenClosedCriterion(preferences2));

        // Reopen Clear Browsing Data preferences and clear history once again.
        setDataTypesToClear(Arrays.asList(DialogOption.CLEAR_HISTORY));
        final Preferences preferences3 =
                startPreferences(ClearBrowsingDataPreferences.class.getName());
        ThreadUtils.runOnUiThreadBlocking(
                new OpenPreferencesEnableDialogAndClickClearRunnable(preferences3));

        // The dialog about other forms of browsing history is still enabled, and history has been
        // selected for deletion. However, the dialog has already been shown before, and therefore
        // we won't show it again. Expect that the preference screen closes.
        CriteriaHelper.pollUiThread(new PreferenceScreenClosedCriterion(preferences3));
    }

    /** This presses the 'clear' button on the root preference page. */
    private Runnable getPressClearRunnable(final ClearBrowsingDataPreferences preferences) {
        return new Runnable() {
            @Override
            public void run() {
                PreferenceScreen screen = preferences.getPreferenceScreen();
                ButtonPreference clearButton = (ButtonPreference) screen.findPreference(
                        ClearBrowsingDataPreferences.PREF_CLEAR_BUTTON);
                assertTrue(clearButton.isEnabled());
                clearButton.getOnPreferenceClickListener().onPreferenceClick(clearButton);
            }
        };
    }

    /** This presses the clear button in the important sites dialog */
    private Runnable getPressButtonInImportantDialogRunnable(
            final ClearBrowsingDataPreferences preferences, final int whichButton) {
        return new Runnable() {
            @Override
            public void run() {
                assertNotNull(preferences);
                ConfirmImportantSitesDialogFragment dialog =
                        preferences.getImportantSitesDialogFragment();
                ((AlertDialog) dialog.getDialog()).getButton(whichButton).performClick();
            }
        };
    }

    /**
     * This waits until the important dialog fragment & the given number of important sites are
     * shown.
     */
    private void waitForImportantDialogToShow(final ClearBrowsingDataPreferences preferences,
            final int numImportantSites) throws Exception {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                assertNotNull(preferences);
                if (preferences.getImportantSitesDialogFragment() == null
                        || !preferences.getImportantSitesDialogFragment().getDialog().isShowing()) {
                    return false;
                }
                ListView sitesList = preferences.getImportantSitesDialogFragment().getSitesList();
                return sitesList.getAdapter().getCount() == numImportantSites;
            }
        });
    }

    /** This runnable marks the given origins as important. */
    private Runnable getMarkOriginsAsImportantRunnable(final String[] importantOrigins) {
        return new Runnable() {
            @Override
            public void run() {
                for (String origin : importantOrigins) {
                    PrefServiceBridge.markOriginAsImportantForTesting(origin);
                }
            }
        };
    }

    /**
     * Tests that the important sites dialog is shown, and if we don't deselect anything we
     * correctly clear everything.
     */
    @CommandLineFlags.Add({"enable-features=ImportantSitesInCBD", "enable-site-engagement"})
    @MediumTest
    @Feature({"SiteEngagement"})
    public void testImportantSitesDialogNoFiltering() throws Exception {
        // Sign in.
        SigninTestUtil.addAndSignInTestAccount();
        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.IMPORTANT_SITES_IN_CBD));

        final String testUrl =
                mTestServer.getURL("/chrome/test/data/android/storage_persistance.html");
        final String serverOrigin = mTestServer.getURL("/");
        final String[] importantOrigins = {"http://www.facebook.com", serverOrigin};
        // First mark our origins as important.
        ThreadUtils.runOnUiThreadBlocking(getMarkOriginsAsImportantRunnable(importantOrigins));

        // Load the page and clear any set storage.
        loadUrl(testUrl + "#clear");
        assertEquals("false", runJavaScriptCodeInCurrentTab("hasAllStorage()"));
        runJavaScriptCodeInCurrentTab("setStorage()");
        assertEquals("true", runJavaScriptCodeInCurrentTab("hasAllStorage()"));

        // Load the page again and ensure the cookie still is set.
        loadUrl(testUrl);
        assertEquals("true", runJavaScriptCodeInCurrentTab("hasAllStorage()"));

        ClearBrowsingDataPreferences preferences =
                (ClearBrowsingDataPreferences) startPreferences(
                        ClearBrowsingDataPreferences.class.getName())
                        .getFragmentForTest();

        // Clear in root preference.
        ThreadUtils.runOnUiThreadBlocking(getPressClearRunnable(preferences));
        // Check that the important sites dialog is shown, and the list is visible.
        waitForImportantDialogToShow(preferences, 2);
        // Clear in important dialog.
        ThreadUtils.runOnUiThreadBlocking(
                getPressButtonInImportantDialogRunnable(preferences, AlertDialog.BUTTON_POSITIVE));
        waitForProgressToComplete(preferences);

        // Verify we don't have storage.
        loadUrl(testUrl);
        assertEquals("false", runJavaScriptCodeInCurrentTab("hasAllStorage()"));
    }

    /**
     * Tests that the important sites dialog is shown and if we cancel nothing happens.
     */
    @CommandLineFlags.Add({"enable-features=ImportantSitesInCBD", "enable-site-engagement"})
    @MediumTest
    @Feature({"SiteEngagement"})
    public void testImportantSitesDialogNoopOnCancel() throws Exception {
        // Sign in.
        SigninTestUtil.addAndSignInTestAccount();
        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.IMPORTANT_SITES_IN_CBD));

        final String testUrl =
                mTestServer.getURL("/chrome/test/data/android/storage_persistance.html");
        final String serverOrigin = mTestServer.getURL("/");
        final String[] importantOrigins = {"http://www.facebook.com", serverOrigin};
        // First mark our origins as important.
        ThreadUtils.runOnUiThreadBlocking(getMarkOriginsAsImportantRunnable(importantOrigins));

        // Load the page and clear any set storage.
        loadUrl(testUrl + "#clear");
        assertEquals("false", runJavaScriptCodeInCurrentTab("hasAllStorage()"));
        runJavaScriptCodeInCurrentTab("setStorage()");
        assertEquals("true", runJavaScriptCodeInCurrentTab("hasAllStorage()"));

        Preferences preferences = startPreferences(ClearBrowsingDataPreferences.class.getName());
        ClearBrowsingDataPreferences fragment =
                (ClearBrowsingDataPreferences) preferences.getFragmentForTest();
        ThreadUtils.runOnUiThreadBlocking(getPressClearRunnable(fragment));
        // Check that the important sites dialog is shown, and the list is visible.
        waitForImportantDialogToShow(fragment, 2);
        // Press the cancel button.
        ThreadUtils.runOnUiThreadBlocking(
                getPressButtonInImportantDialogRunnable(fragment, AlertDialog.BUTTON_NEGATIVE));
        preferences.finish();
        loadUrl(testUrl);
        assertEquals("true", runJavaScriptCodeInCurrentTab("hasAllStorage()"));
    }

    /**
     * Tests that the important sites dialog is shown, we can successfully uncheck options, and
     * clicking clear doesn't clear the protected domain.
     */
    @CommandLineFlags.Add({"enable-features=ImportantSitesInCBD", "enable-site-engagement"})
    @MediumTest
    @Feature({"SiteEngagement"})
    public void testImportantSitesDialog() throws Exception {
        // Sign in.
        SigninTestUtil.addAndSignInTestAccount();
        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.IMPORTANT_SITES_IN_CBD));

        final String testUrl =
                mTestServer.getURL("/chrome/test/data/android/storage_persistance.html");
        final String serverOrigin = mTestServer.getURL("/");
        final String serverHost = new URL(testUrl).getHost();
        final String[] importantOrigins = {"http://www.facebook.com", serverOrigin};

        // First mark our origins as important.
        ThreadUtils.runOnUiThreadBlocking(getMarkOriginsAsImportantRunnable(importantOrigins));

        // Load the page and clear any set storage.
        loadUrl(testUrl + "#clear");
        assertEquals("false", runJavaScriptCodeInCurrentTab("hasAllStorage()"));
        runJavaScriptCodeInCurrentTab("setStorage()");
        assertEquals("true", runJavaScriptCodeInCurrentTab("hasAllStorage()"));

        final Preferences preferences =
                startPreferences(ClearBrowsingDataPreferences.class.getName());
        final ClearBrowsingDataPreferences fragment =
                (ClearBrowsingDataPreferences) preferences.getFragmentForTest();

        // Uncheck the first item (our internal web server).
        ThreadUtils.runOnUiThreadBlocking(getPressClearRunnable(fragment));
        waitForImportantDialogToShow(fragment, 2);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ListView sitesList = fragment.getImportantSitesDialogFragment().getSitesList();
                sitesList.performItemClick(
                        sitesList.getChildAt(0), 0, sitesList.getAdapter().getItemId(0));
            }
        });

        // Check that our server origin is in the set of deselected domains.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ConfirmImportantSitesDialogFragment dialog =
                        fragment.getImportantSitesDialogFragment();
                return dialog.getDeselectedDomains().contains(serverHost);
            }
        });

        // Click the clear button.
        ThreadUtils.runOnUiThreadBlocking(
                getPressButtonInImportantDialogRunnable(fragment, AlertDialog.BUTTON_POSITIVE));

        waitForProgressToComplete(fragment);
        // And check we didn't clear our cookies.
        assertEquals("true", runJavaScriptCodeInCurrentTab("hasAllStorage()"));
    }

    private void setDataTypesToClear(final List<DialogOption> typesToClear) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                for (DialogOption option : DialogOption.values()) {
                    boolean enabled = typesToClear.contains(option);
                    PrefServiceBridge.getInstance().setBrowsingDataDeletionPreference(
                            option.getDataType(), enabled);
                }
            }
        });
    }
}
