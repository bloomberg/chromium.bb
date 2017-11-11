// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertThat;

import static org.chromium.chrome.test.BottomSheetTestRule.waitForWindowUpdates;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.MethodRule;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.MethodParamAnnotationRule;
import org.chromium.base.test.params.ParameterAnnotations.MethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameterBefore;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NtpUiCaptureTestData;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.ui.test.util.UiRestriction;

import java.util.Arrays;
import java.util.List;

/**
 * Tests for the appearance of the special states of the home sheet.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
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

    @Rule
    public MethodRule mMethodParamAnnotationProcessor = new MethodParamAnnotationRule();

    private static final String SIGNIN_PROMO = "SigninPromo";
    @MethodParameter(SIGNIN_PROMO)
    private static List<ParameterSet> sSigninPromoParams = Arrays.asList(
            new ParameterSet().name("SigninPromoDisabled").value(false),
            new ParameterSet().name("SigninPromoEnabled").value(true)
    );

    @UseMethodParameterBefore(SIGNIN_PROMO)
    public void applyEnableSigninPromoParam(boolean enableSigninPromo) {
        if (enableSigninPromo) {
            Features.getInstance().enable(ChromeFeatureList.ANDROID_SIGNIN_PROMOS);
        } else {
            Features.getInstance().disable(ChromeFeatureList.ANDROID_SIGNIN_PROMOS);
        }
    }

    @Before
    public void setup() throws InterruptedException {
        mActivityRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature({"UiCatalogue"})
    @ScreenShooter.Directory("SignInPromo")
    @UseMethodParameter(SIGNIN_PROMO)
    public void testSignInPromo(boolean signinPromoEnabled) {
        assertThat(ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_SIGNIN_PROMOS),
                is(signinPromoEnabled));

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
    @ScreenShooter.Directory("NewTab")
    public void testNewTab() {
        // Select "New tab" from the menu.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
        waitForWindowUpdates();

        mScreenShooter.shoot("NewTab");
    }
}
