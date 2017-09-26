// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.longClick;
import static android.support.test.espresso.contrib.RecyclerViewActions.actionOnItemAtPosition;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.chrome.test.BottomSheetTestRule.waitForWindowUpdates;

import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NtpUiCaptureTestData;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests for the appearance of the card suggestions in the home sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public class HomeSheetCardsUiCaptureTest {
    @Rule
    public SuggestionsBottomSheetTestRule mActivityRule = new SuggestionsBottomSheetTestRule();

    @Rule
    public SuggestionsDependenciesRule setupSuggestions() {
        SuggestionsDependenciesRule.TestFactory depsFactory = NtpUiCaptureTestData.createFactory();
        FakeSuggestionsSource suggestionsSource = new FakeSuggestionsSource();
        NtpUiCaptureTestData.registerArticleSamples(suggestionsSource);
        depsFactory.suggestionsSource = suggestionsSource;
        return new SuggestionsDependenciesRule(depsFactory);
    }

    @Rule
    public ScreenShooter mScreenShooter = new ScreenShooter();

    @Before
    public void setup() throws InterruptedException {
        ChromePreferenceManager.getInstance().setNewTabPageGenericSigninPromoDismissed(true);
        mActivityRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature({"UiCatalogue"})
    @ScreenShooter.Directory("HomeSheetCards")
    public void testContextMenu() throws Exception {
        mActivityRule.setSheetState(BottomSheet.SHEET_STATE_FULL, false);
        waitForWindowUpdates();

        int position = mActivityRule.getFirstPositionForType(ItemViewType.SNIPPET);
        onView(withId(R.id.recycler_view)).perform(actionOnItemAtPosition(position, longClick()));
        mScreenShooter.shoot("ContextMenu");
    }

    @Test
    @MediumTest
    @Feature({"UiCatalogue"})
    @ScreenShooter.Directory("HomeSheetCards")
    public void testScrolling() throws Exception {
        mActivityRule.setSheetState(BottomSheet.SHEET_STATE_FULL, false);
        waitForWindowUpdates();

        mActivityRule.scrollToFirstItemOfType(ItemViewType.ACTION);
        waitForWindowUpdates();
        mScreenShooter.shoot("ScrolledToMoreButton");

        mActivityRule.scrollToFirstItemOfType(ItemViewType.SNIPPET);
        waitForWindowUpdates();
        mScreenShooter.shoot("ScrolledToFirstCard");
    }
}
