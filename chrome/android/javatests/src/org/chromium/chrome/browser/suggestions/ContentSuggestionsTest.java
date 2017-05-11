// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.NewTabPageTestUtils;

/**
 * Misc. Content Suggestions instrumentation tests.
 */
public class ContentSuggestionsTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    public ContentSuggestionsTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @SmallTest
    @Feature("Suggestions")
    @CommandLineFlags.Add("enable-features=ContentSuggestionsSettings")
    public void testRemoteSuggestionsEnabled() throws InterruptedException {
        NewTabPage ntp = loadNTPWithSearchSuggestState(true);
        SuggestionsUiDelegate uiDelegate = ntp.getManagerForTesting();
        assertTrue(isCategoryEnabled(uiDelegate.getSuggestionsSource(), KnownCategories.ARTICLES));
    }

    @SmallTest
    @Feature("Suggestions")
    @CommandLineFlags.Add("enable-features=ContentSuggestionsSettings")
    public void testRemoteSuggestionsDisabled() throws InterruptedException {
        NewTabPage ntp = loadNTPWithSearchSuggestState(false);
        SuggestionsUiDelegate uiDelegate = ntp.getManagerForTesting();
        assertFalse(isCategoryEnabled(uiDelegate.getSuggestionsSource(), KnownCategories.ARTICLES));
    }

    private NewTabPage loadNTPWithSearchSuggestState(final boolean enabled)
            throws InterruptedException {
        Tab tab = getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setSearchSuggestEnabled(enabled);
            }
        });

        loadUrl(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(tab);

        assertTrue(tab.getNativePage() instanceof NewTabPage);
        return (NewTabPage) tab.getNativePage();
    }

    private boolean isCategoryEnabled(SuggestionsSource source, @CategoryInt int category) {
        int[] categories = source.getCategories();
        for (int cat : categories) {
            if (cat != category) continue;
            if (SnippetsBridge.isCategoryEnabled(source.getCategoryStatus(category))) return true;
        }

        return false;
    }
}
