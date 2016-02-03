// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.preference.PreferenceScreen;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.preferences.ButtonPreference;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.Arrays;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.Set;

/**
 * Peforms integration tests with ClearBrowsingDataPreferences.
 */
public class ClearBrowsingDataPreferencesTest
        extends ChromeActivityTestCaseBase<ChromeActivity> {

    private boolean mCallbackCalled;

    public ClearBrowsingDataPreferencesTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @MediumTest
    public void testClearingSiteDataClearsWebapps() throws Exception {
        WebappRegistry.registerWebapp(getActivity(), "first");
        WebappRegistry.getRegisteredWebappIds(getActivity(), new WebappRegistry.FetchCallback() {
            @Override
            public void onWebappIdsRetrieved(Set<String> ids) {
                assertEquals(new HashSet<String>(Arrays.asList("first")), ids);
                mCallbackCalled = true;
            }
        });
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCallbackCalled;
            }
        });
        mCallbackCalled = false;

        final Preferences preferences =
                startPreferences(TestClearBrowsingDataPreferences.class.getName());

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
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
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
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCallbackCalled;
            }
        });
    }

    /**
     * A testing version of ClearBrowsingDataPreferences that preselects the cookies option.
     * Must be public, as ChromeActivityTestCaseBase.startPreferences references it by name.
     */
    public static class TestClearBrowsingDataPreferences extends ClearBrowsingDataPreferences {
        private static final EnumSet<DialogOption> DEFAULT_OPTIONS = EnumSet.of(
                ClearBrowsingDataPreferences.DialogOption.CLEAR_COOKIES_AND_SITE_DATA);

        @Override
        protected boolean isOptionSelectedByDefault(DialogOption option) {
            return DEFAULT_OPTIONS.contains(option);
        }
    }
}
