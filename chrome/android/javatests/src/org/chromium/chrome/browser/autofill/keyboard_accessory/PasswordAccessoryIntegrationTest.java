// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.assertThat;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.notNullValue;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.createEmptyCredentials;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.createUserInfo;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;

import android.support.test.espresso.matcher.BoundedMatcher;
import android.support.test.filters.SmallTest;
import android.text.method.PasswordTransformationMethod;
import android.view.View;
import android.widget.TextView;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.FooterCommand;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.UserInfo.Field;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;
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

    @Before
    public void setUp() throws Exception {
        // TODO(crbug.com/894428) - fix this suite to use the embedded test server instead of
        // data urls.
        Features.getInstance().enable(ChromeFeatureList.AUTOFILL_ALLOW_NON_HTTP_ACTIVATION);
    }

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

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));

        AccessorySheetData accessorySheetData = new AccessorySheetData("Passwords");
        accessorySheetData.getUserInfoList().add(
                createUserInfo("mayapark@gmail.com", "SomeHiddenPassword"));
        accessorySheetData.getUserInfoList().add(
                createUserInfo("mayaelisabethmercedesgreenepark@googlemail.com",
                        "ExtremelyLongPasswordThatUsesQuiteSomeSpaceInTheSheet"));
        mHelper.sendCredentials(accessorySheetData);

        // Check that the provided elements are there.
        whenDisplayed(withText("Passwords"));
        whenDisplayed(withText("mayapark@gmail.com"));
        whenDisplayed(withText("SomeHiddenPassword")).check(matches(isTransformed()));
        // TODO(fhorschig): Figure out whether the long name should be cropped or wrapped.
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/920901")
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    public void testPasswordSheetDisplaysNoPasswordsMessageAndOptions()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        final AtomicReference<FooterCommand> clicked = new AtomicReference<>();

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));

        AccessorySheetData accessorySheetData =
                new AccessorySheetData("No saved passwords for abc.com");
        accessorySheetData.getFooterCommands().add(
                new FooterCommand("Suggest strong password...", clicked::set));
        accessorySheetData.getFooterCommands().add(
                new FooterCommand("Manage passwords...", clicked::set));
        mHelper.sendCredentials(accessorySheetData);

        // Scroll down and click the suggestion.
        whenDisplayed(withText("Suggest strong password...")).perform(click());

        // The callback should have triggered and set the reference to the selected Item.
        assertThat(clicked.get(), notNullValue());
        assertThat(clicked.get().getDisplayText(), equalTo("Suggest strong password..."));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    public void testPasswordSheetTriggersCallback() throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        final AtomicReference<Field> clicked = new AtomicReference<>();

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));

        AccessorySheetData accessorySheetData = new AccessorySheetData("Passwords");
        accessorySheetData.getUserInfoList().add(
                createUserInfo("mpark@abc.com", "ShorterPassword", clicked::set));
        accessorySheetData.getUserInfoList().add(createUserInfo("mayap@xyz.com", "PWD"));
        accessorySheetData.getUserInfoList().add(createUserInfo("park@googlemail.com", "P@$$W0rt"));
        accessorySheetData.getUserInfoList().add(
                createUserInfo("mayapark@gmail.com", "SomeHiddenLongPassword"));
        mHelper.sendCredentials(accessorySheetData);

        // Click the suggestion.
        whenDisplayed(withText("mpark@abc.com")).perform(click());

        // The callback should have triggered and set the reference to the selected Item.
        assertThat(clicked.get(), notNullValue());
        assertThat(clicked.get().getDisplayText(), equalTo("mpark@abc.com"));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    public void testDisplaysEmptyStateMessageWithoutSavedPasswords()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.sendCredentials(createEmptyCredentials());
        mHelper.waitForKeyboard();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));
        onView(withText(containsString("No Saved passwords"))).check(matches(isDisplayed()));
    }

    /**
     * Matches any {@link TextView} which applies a {@link PasswordTransformationMethod}.
     * @return The matcher checking the transformation method.
     */
    private static Matcher<View> isTransformed() {
        return new BoundedMatcher<View, TextView>(TextView.class) {
            @Override
            public boolean matchesSafely(TextView textView) {
                return textView.getTransformationMethod() instanceof PasswordTransformationMethod;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("is a transformed password.");
            }
        };
    }
}
