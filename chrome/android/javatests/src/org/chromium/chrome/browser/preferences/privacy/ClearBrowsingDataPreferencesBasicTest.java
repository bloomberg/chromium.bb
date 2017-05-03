// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertThat;

import android.content.Context;
import android.preference.CheckBoxPreference;
import android.preference.PreferenceScreen;
import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;

/**
 * Integration tests for ClearBrowsingDataPreferencesBasic.
 */
public class ClearBrowsingDataPreferencesBasicTest
        extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String GOOGLE_ACCOUNT = "Google Account";
    private static final String OTHER_ACTIVITY = "other activity";
    private static final String SIGNED_IN_DEVICES = "signed-in devices";

    public ClearBrowsingDataPreferencesBasicTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        SigninTestUtil.setUpAuthForTest(getInstrumentation());
        startMainActivityOnBlankPage();
    }

    @Override
    protected void tearDown() throws Exception {
        ProfileSyncService.overrideForTests(null);
        super.tearDown();
    }

    private static class StubProfileSyncService extends ProfileSyncService {
        StubProfileSyncService() {
            super();
        }
    }

    private void setSyncable(boolean syncable) {
        Context context = getInstrumentation().getTargetContext();
        MockSyncContentResolverDelegate delegate = new MockSyncContentResolverDelegate();
        delegate.setMasterSyncAutomatically(syncable);
        AndroidSyncSettings.overrideForTests(context, delegate);
        if (syncable) {
            AndroidSyncSettings.enableChromeSync(context);
        } else {
            AndroidSyncSettings.disableChromeSync(context);
        }

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProfileSyncService.overrideForTests(new StubProfileSyncService());
            }
        });
    }

    private String getCheckboxSummary(PreferenceScreen screen, String preference) {
        CheckBoxPreference checkbox = (CheckBoxPreference) screen.findPreference(preference);
        return new StringBuilder(checkbox.getSummary()).toString();
    }

    /**
     * Tests that for users who are not signed in, only the general information is shown.
     */
    @SmallTest
    public void testCheckBoxTextNonsigned() throws Exception {
        SigninTestUtil.resetSigninState();

        final Preferences preferences =
                startPreferences(ClearBrowsingDataPreferencesBasic.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClearBrowsingDataPreferencesBasic fragment =
                        (ClearBrowsingDataPreferencesBasic) preferences.getFragmentForTest();
                PreferenceScreen screen = fragment.getPreferenceScreen();

                String cookiesSummary =
                        getCheckboxSummary(screen, ClearBrowsingDataPreferencesBasic.PREF_COOKIES);
                String historySummary =
                        getCheckboxSummary(screen, ClearBrowsingDataPreferencesBasic.PREF_HISTORY);

                assertThat(cookiesSummary, not(containsString(GOOGLE_ACCOUNT)));
                assertThat(historySummary, not(containsString(OTHER_ACTIVITY)));
                assertThat(historySummary, not(containsString(SIGNED_IN_DEVICES)));
            }
        });
    }

    /**
     * Tests that for users who are signed in but don't have sync activated, only information about
     * your "google account" which will stay signed in and "other activity" is shown.
     */
    @SmallTest
    public void testCheckBoxTextSigned() throws Exception {
        SigninTestUtil.addAndSignInTestAccount();
        setSyncable(false);

        final Preferences preferences =
                startPreferences(ClearBrowsingDataPreferencesBasic.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClearBrowsingDataPreferencesBasic fragment =
                        (ClearBrowsingDataPreferencesBasic) preferences.getFragmentForTest();
                PreferenceScreen screen = fragment.getPreferenceScreen();

                String cookiesSummary =
                        getCheckboxSummary(screen, ClearBrowsingDataPreferencesBasic.PREF_COOKIES);
                String historySummary =
                        getCheckboxSummary(screen, ClearBrowsingDataPreferencesBasic.PREF_HISTORY);

                assertThat(cookiesSummary, containsString(GOOGLE_ACCOUNT));
                assertThat(historySummary, containsString(OTHER_ACTIVITY));
                assertThat(historySummary, not(containsString(SIGNED_IN_DEVICES)));
            }
        });
    }

    /**
     * Tests that users who are signed in, and have sync enabled see information about their
     * "google account", "other activity" and history on "signed in devices".
     */
    @SmallTest
    public void testCheckBoxTextSignedAndSynced() throws Exception {
        SigninTestUtil.addAndSignInTestAccount();
        setSyncable(true);

        final Preferences preferences =
                startPreferences(ClearBrowsingDataPreferencesBasic.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClearBrowsingDataPreferencesBasic fragment =
                        (ClearBrowsingDataPreferencesBasic) preferences.getFragmentForTest();
                PreferenceScreen screen = fragment.getPreferenceScreen();

                String cookiesSummary =
                        getCheckboxSummary(screen, ClearBrowsingDataPreferencesBasic.PREF_COOKIES);
                String historySummary =
                        getCheckboxSummary(screen, ClearBrowsingDataPreferencesBasic.PREF_HISTORY);

                assertThat(cookiesSummary, containsString(GOOGLE_ACCOUNT));
                assertThat(historySummary, containsString(OTHER_ACTIVITY));
                assertThat(historySummary, containsString(SIGNED_IN_DEVICES));
            }
        });
    }
}
