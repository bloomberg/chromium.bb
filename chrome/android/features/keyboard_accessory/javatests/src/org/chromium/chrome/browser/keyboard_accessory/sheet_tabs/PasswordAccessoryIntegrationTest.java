// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;

import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.isTransformed;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;

import android.os.Build;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.concurrent.TimeoutException;
/**
 * Integration tests for password accessory views.
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

        CriteriaHelper.pollUiThread(() -> {
            return mHelper.getOrCreatePasswordAccessorySheet() != null;
        }, " Password Sheet should be bound to accessory sheet.");
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    public void testPasswordSheetUnavailableWithoutFeature() throws InterruptedException {
        mHelper.loadTestPage(false);

        Assert.assertNull("Password Sheet should not have been created.",
                mHelper.getOrCreatePasswordAccessorySheet());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP, message = "crbug.com/958631")
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
        whenDisplayed(withId(R.id.passwords_sheet));
        onView(withText(containsString("Manage password"))).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP, message = "crbug.com/958631")
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
        whenDisplayed(withId(R.id.passwords_sheet));
        onView(withText(containsString("No saved passwords"))).check(matches(isDisplayed()));
    }
}
