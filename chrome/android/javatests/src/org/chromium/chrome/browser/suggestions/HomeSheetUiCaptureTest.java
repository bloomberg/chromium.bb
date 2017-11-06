// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.junit.Assert.assertNotEquals;

import static org.chromium.chrome.test.BottomSheetTestRule.waitForWindowUpdates;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.parameter.CommandLineParameter;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NtpUiCaptureTestData;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.ChromeHome;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests for the appearance of the special states of the home sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
// TODO(https://crbug.com/754778) improve annotation processor. We need to remove the currently
// registered Feature flags to be able to change them later.
@CommandLineFlags.Remove(ChromeHome.ENABLE_FLAGS)
@ScreenShooter.Directory("HomeSheetStates")
public class HomeSheetUiCaptureTest {
    @Rule
    public SuggestionsBottomSheetTestRule mActivityRule = new SuggestionsBottomSheetTestRule();

    @Rule
    public SuggestionsDependenciesRule setupSuggestions() {
        return new SuggestionsDependenciesRule(NtpUiCaptureTestData.createFactory());
    }

    @Rule
    public ScreenShooter mScreenShooter = new ScreenShooter();

    @Before
    public void setup() throws InterruptedException {
        mActivityRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature({"UiCatalogue"})
    @CommandLineParameter({
            ChromeHome.ENABLE_FLAGS,
            "enable-features=" + ChromeHome.FEATURES + "," + ChromeFeatureList.ANDROID_SIGNIN_PROMOS
    })
    @ScreenShooter.Directory("SignInPromo")
    public void testSignInPromo() {
        // Needs to be "Full" to for this to work on small screens in landscape.
        mActivityRule.setSheetState(BottomSheet.SHEET_STATE_FULL, false);
        waitForWindowUpdates();

        mActivityRule.scrollToFirstItemOfType(ItemViewType.PROMO);

        boolean newSigninPromo =
                ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_SIGNIN_PROMOS);
        mScreenShooter.shoot("SignInPromo" + (newSigninPromo ? "_new" : ""));
    }

    @Test
    @MediumTest
    @Feature({"UiCatalogue"})
    @CommandLineFlags.Add(ChromeHome.ENABLE_FLAGS)
    @ScreenShooter.Directory("AllDismissed")
    public void testAllDismissed() {
        NewTabPageAdapter adapter = mActivityRule.getAdapter();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            int signInPromoPosition = adapter.getFirstPositionForType(ItemViewType.PROMO);
            assertNotEquals(signInPromoPosition, RecyclerView.NO_POSITION);
            adapter.dismissItem(signInPromoPosition, ignored -> { });

            // Dismiss all articles.
            while (true) {
                int articlePosition = adapter.getFirstPositionForType(ItemViewType.SNIPPET);
                if (articlePosition == RecyclerView.NO_POSITION) break;
                adapter.dismissItem(articlePosition, ignored -> { });
            }
        });

        mActivityRule.scrollToFirstItemOfType(ItemViewType.ALL_DISMISSED);

        mScreenShooter.shoot("All_dismissed");
    }

    @Test
    @MediumTest
    @Feature({"UiCatalogue"})
    @CommandLineFlags.Add(ChromeHome.ENABLE_FLAGS)
    @ScreenShooter.Directory("NewTab")
    public void testNewTab() {
        // Select "New tab" from the menu.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
        waitForWindowUpdates();

        mScreenShooter.shoot("NewTab");
    }
}
