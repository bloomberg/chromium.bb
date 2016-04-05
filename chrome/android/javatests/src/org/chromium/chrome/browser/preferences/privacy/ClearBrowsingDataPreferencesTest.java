// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.preference.CheckBoxPreference;
import android.preference.Preference;
import android.preference.PreferenceScreen;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.SpannableString;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.preferences.ButtonPreference;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.privacy.ClearBrowsingDataPreferences.DialogOption;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Integration tests for ClearBrowsingDataPreferences.
 */
public class ClearBrowsingDataPreferencesTest
        extends ChromeActivityTestCaseBase<ChromeActivity> {

    private boolean mCallbackCalled;

    public ClearBrowsingDataPreferencesTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        SigninTestUtil.setUpAuthForTest(getInstrumentation());
        startMainActivityOnBlankPage();
    }

    /**
     * Tests that web apps are cleared when the "cookies and site data" option is selected.
     */
    @MediumTest
    public void testClearingSiteDataClearsWebapps() throws Exception {
        WebappRegistry.registerWebapp(getActivity(), "first", "https://www.google.com");
        WebappRegistry.getRegisteredWebappIds(getActivity(), new WebappRegistry.FetchCallback() {
            @Override
            public void onWebappIdsRetrieved(Set<String> ids) {
                assertEquals(new HashSet<String>(Arrays.asList("first")), ids);
                mCallbackCalled = true;
            }
        });
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCallbackCalled;
            }
        });
        mCallbackCalled = false;

        setDataTypesToClear(Arrays.asList(DialogOption.CLEAR_COOKIES_AND_SITE_DATA));
        final Preferences preferences =
                startPreferences(ClearBrowsingDataPreferences.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClearBrowsingDataPreferences fragment =
                        (ClearBrowsingDataPreferences) preferences.getFragmentForTest();
                PreferenceScreen screen = fragment.getPreferenceScreen();
                ButtonPreference clearButton = (ButtonPreference) screen.findPreference(
                        ClearBrowsingDataPreferences.PREF_CLEAR_BUTTON);
                clearButton.getOnPreferenceClickListener().onPreferenceClick(clearButton);
            }
        });
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ClearBrowsingDataPreferences fragment =
                        (ClearBrowsingDataPreferences) preferences.getFragmentForTest();
                return fragment.getProgressDialog() == null;
            }
        });

        WebappRegistry.getRegisteredWebappIds(getActivity(), new WebappRegistry.FetchCallback() {
            @Override
            public void onWebappIdsRetrieved(Set<String> ids) {
                assertTrue(ids.isEmpty());
                mCallbackCalled = true;
            }
        });
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCallbackCalled;
            }
        });
    }

    /**
     * Tests that web app scopes and last launch times are cleared when the "history" option is
     * selected. However, the web app is not removed from the registry.
     */
    @MediumTest
    public void testClearingHistoryClearsWebappScopesAndLaunchTimes() throws Exception {
        WebappRegistry.registerWebapp(getActivity(), "first", "https://www.google.com");
        WebappRegistry.getRegisteredWebappIds(getActivity(), new WebappRegistry.FetchCallback() {
            @Override
            public void onWebappIdsRetrieved(Set<String> ids) {
                assertEquals(new HashSet<String>(Arrays.asList("first")), ids);
                mCallbackCalled = true;
            }
        });
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCallbackCalled;
            }
        });
        mCallbackCalled = false;

        setDataTypesToClear(Arrays.asList(DialogOption.CLEAR_HISTORY));
        final Preferences preferences =
                startPreferences(ClearBrowsingDataPreferences.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClearBrowsingDataPreferences fragment =
                        (ClearBrowsingDataPreferences) preferences.getFragmentForTest();
                PreferenceScreen screen = fragment.getPreferenceScreen();
                ButtonPreference clearButton = (ButtonPreference) screen.findPreference(
                        ClearBrowsingDataPreferences.PREF_CLEAR_BUTTON);
                clearButton.getOnPreferenceClickListener().onPreferenceClick(clearButton);
            }
        });
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ClearBrowsingDataPreferences fragment =
                        (ClearBrowsingDataPreferences) preferences.getFragmentForTest();
                return fragment.getProgressDialog() == null;
            }
        });

        // The web app should still exist in the registry.
        WebappRegistry.getRegisteredWebappIds(getActivity(), new WebappRegistry.FetchCallback() {
            @Override
            public void onWebappIdsRetrieved(Set<String> ids) {
                assertEquals(new HashSet<String>(Arrays.asList("first")), ids);
                mCallbackCalled = true;
            }
        });
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCallbackCalled;
            }
        });
        mCallbackCalled = false;

        // Scope should be empty.
        WebappDataStorage.getScope(getActivity(), "first",
                new WebappDataStorage.FetchCallback<String>() {
                    @Override
                    public void onDataRetrieved(String readObject) {
                        assertEquals(readObject, "");
                        mCallbackCalled = true;
                    }
                }
        );
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCallbackCalled;
            }
        });
        mCallbackCalled = false;

        // The last used time should be 0.
        WebappDataStorage.getLastUsedTime(getActivity(), "first",
                new WebappDataStorage.FetchCallback<Long>() {
                    @Override
                    public void onDataRetrieved(Long readObject) {
                        long lastUsed = readObject;
                        assertEquals(lastUsed, 0);
                        mCallbackCalled = true;
                    }
                }
        );
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCallbackCalled;
            }
        });
    }

    /**
     * Tests that a fragment with all options preselected indeed has all checkboxes checked
     * on startup, and that deletion with all checkboxes checked completes successfully.
     */
    @MediumTest
    public void testClearingEverything() throws Exception {
        setDataTypesToClear(Arrays.asList(DialogOption.values()));

        final Preferences preferences =
                startPreferences(ClearBrowsingDataPreferences.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClearBrowsingDataPreferences fragment =
                        (ClearBrowsingDataPreferences) preferences.getFragmentForTest();
                PreferenceScreen screen = fragment.getPreferenceScreen();

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

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ClearBrowsingDataPreferences fragment =
                        (ClearBrowsingDataPreferences) preferences.getFragmentForTest();
                return fragment.getProgressDialog() == null;
            }
        });
    }

    /**
     * Tests that for users who are not signed in, only the general footnote is shown.
     */
    @SmallTest
    public void testFooterNonsigned() throws Exception {
        SigninTestUtil.get().resetSigninState();

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
        SigninTestUtil.get().addAndSignInTestAccount();

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
