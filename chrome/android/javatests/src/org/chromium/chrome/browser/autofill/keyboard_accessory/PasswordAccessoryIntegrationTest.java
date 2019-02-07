// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.isTransformed;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.concurrent.TimeoutException;
/**
 * Integration tests for password accessory views. This integration test currently stops testing at
 * the bridge - ideally, there should be an easy way to add a temporary account with temporary
 * passwords.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordAccessoryIntegrationTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    @After
    public void tearDown() {
        mHelper.clear();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    public void testPasswordSheetIsAvailable() throws InterruptedException {
        mHelper.loadTestPage(false);

        Assert.assertNotNull("Password Sheet should be bound to accessory sheet.",
                mActivityTestRule.getActivity()
                        .getManualFillingController()
                        .getMediatorForTesting()
                        .getPasswordAccessorySheet());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EXPERIMENTAL_UI})
    public void testPasswordSheetIsAvailableInExperimentalUi() throws InterruptedException {
        mHelper.loadTestPage(false);

        Assert.assertNotNull("Password Sheet should be bound to accessory sheet.",
                mActivityTestRule.getActivity()
                        .getManualFillingController()
                        .getMediatorForTesting()
                        .getPasswordAccessorySheet());
    }

    @Test
    @SmallTest
    @DisableFeatures(
            {ChromeFeatureList.EXPERIMENTAL_UI, ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    public void
    testPasswordSheetUnavailableWithoutFeature() throws InterruptedException {
        mHelper.loadTestPage(false);

        Assert.assertNull("Password Sheet should not have been created.",
                mActivityTestRule.getActivity()
                        .getManualFillingController()
                        .getMediatorForTesting()
                        .getPasswordAccessorySheet());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    public void testPasswordSheetDisplaysProvidedItems()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.cacheCredentials("mayapark@gmail.com", "SomeHiddenPassword");

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));

        // Check that the provided elements are there.
        whenDisplayed(withText("mayapark@gmail.com"));
        whenDisplayed(withText("SomeHiddenPassword")).check(matches(isTransformed()));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    public void testPasswordSheetDisplaysOptions() throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));

        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));
        onView(withText(containsString("Manage password"))).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    public void testFillsPasswordOnTap() throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.cacheCredentials("mpark@abc.com", "ShorterPassword");

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));

        // Click the suggestion.
        whenDisplayed(withText("ShorterPassword")).perform(click());

        // The callback should have triggered and set the reference to the selected Item.
        CriteriaHelper.pollInstrumentationThread(
                () -> mHelper.getPasswordText().equals("ShorterPassword"));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    public void testDisplaysEmptyStateMessageWithoutSavedPasswords()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));
        onView(withText(containsString("No saved passwords"))).check(matches(isDisplayed()));
    }
}
