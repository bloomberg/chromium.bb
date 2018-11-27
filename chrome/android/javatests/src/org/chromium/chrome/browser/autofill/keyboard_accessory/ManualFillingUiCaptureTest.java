// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.TEST_CREDENTIALS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.scrollToLastElement;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;

import android.support.annotation.Nullable;
import android.support.test.espresso.NoMatchingViewException;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.concurrent.TimeoutException;

/**
 * Screenshot test for manual filling views. They ensure that we don't regress on visual details
 * like shadows, padding and RTL differences. Logic integration tests involving all filling
 * components belong into {@link ManualFillingIntegrationTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY,
        ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManualFillingUiCaptureTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final ScreenShooter mScreenShooter = new ScreenShooter();

    private final ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    @After
    public void tearDown() {
        mHelper.clear();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
    @Feature({"KeyboardAccessory", "LTR", "UiCatalogue"})
    public void testCaptureKeyboardAccessoryWithPasswords()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.addGenerationButton();

        mHelper.clickPasswordField();
        mHelper.sendCredentials(TEST_CREDENTIALS);

        waitForActionsInAccessory();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessoryBar");

        mHelper.sendCredentials(TEST_CREDENTIALS);

        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        waitForSuggestionsInSheet();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswords");

        whenDisplayed(withParent(withId(R.id.keyboard_accessory_sheet)))
                .perform(scrollToLastElement());
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsScrolled");
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
    @Feature({"KeyboardAccessory", "RTL", "UiCatalogue"})
    public void testCaptureKeyboardAccessoryWithPasswordsRTL()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(true);
        mHelper.addGenerationButton();
        mHelper.clickPasswordField();
        mHelper.sendCredentials(TEST_CREDENTIALS);

        waitForActionsInAccessory();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessoryBarRTL");

        mHelper.sendCredentials(TEST_CREDENTIALS);

        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        waitForSuggestionsInSheet();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsRTL");

        whenDisplayed(withParent(withId(R.id.keyboard_accessory_sheet)))
                .perform(scrollToLastElement());
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsScrolledRTL");
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
    @Feature({"KeyboardAccessoryModern", "LTR", "UiCatalogue"})
    public void testCaptureKeyboardAccessoryV2WithPasswords()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.addGenerationButton();
        mHelper.clickPasswordField();
        mHelper.addAutofillChips();
        mHelper.sendCredentials(TEST_CREDENTIALS);

        waitForActionsInAccessory();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessoryBarV2");

        mHelper.sendCredentials(TEST_CREDENTIALS);

        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        waitForSuggestionsInSheet();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsV2");

        whenDisplayed(withParent(withId(R.id.keyboard_accessory_sheet)))
                .perform(scrollToLastElement());
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsV2Scrolled");
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
    @Feature({"KeyboardAccessoryModern", "RTL", "UiCatalogue"})
    public void testCaptureKeyboardAccessoryV2WithPasswordsRTL()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(true);
        mHelper.addGenerationButton();
        mHelper.clickPasswordField();
        mHelper.addAutofillChips();
        mHelper.sendCredentials(TEST_CREDENTIALS);

        waitForActionsInAccessory();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessoryBarV2RTL");

        mHelper.sendCredentials(TEST_CREDENTIALS);

        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        waitForSuggestionsInSheet();
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsV2RTL");

        whenDisplayed(withParent(withId(R.id.keyboard_accessory_sheet)))
                .perform(scrollToLastElement());
        waitForUnrelatedChromeUi();
        mScreenShooter.shoot("AccessorySheetPasswordsV2ScrolledRTL");
    }

    private void waitUntilFilled(View view, @Nullable NoMatchingViewException noViewFound) {
        if (noViewFound != null) throw noViewFound;
        assert view instanceof RecyclerView;
        final RecyclerView recyclerView = (RecyclerView) view;
        CriteriaHelper.pollUiThread(() -> {
            if (recyclerView.getChildCount() <= 0) return false;
            View firstChild = recyclerView.getChildAt(0);
            return firstChild.isShown() && !firstChild.isDirty() && !firstChild.isLayoutRequested();
        });
    }

    private void waitForUnrelatedChromeUi() throws InterruptedException {
        whenDisplayed(withId(R.id.home_button));
        Thread.sleep(scaleTimeout(30)); // Reduces flakiness due to delayed events.
    }

    private void waitForActionsInAccessory() throws InterruptedException {
        whenDisplayed(withId(R.id.actions_view));
        onView(withId(R.id.actions_view)).check(this::waitUntilFilled);
    }

    private void waitForSuggestionsInSheet() throws InterruptedException {
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));
        onView(withParent(withId(R.id.keyboard_accessory_sheet))).check(this::waitUntilFilled);
    }
}