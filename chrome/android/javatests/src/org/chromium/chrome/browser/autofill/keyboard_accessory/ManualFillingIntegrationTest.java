// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.assertThat;
import static android.support.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withChild;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertNotNull;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.createTestCredentials;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;

import android.support.test.espresso.Espresso;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.DropdownPopupWindowInterface;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests for password accessory views. This integration test currently stops testing at
 * the bridge - ideally, there should be an easy way to add a temporary account with temporary
 * passwords.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY,
        ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY,
        // TODO(crbug.com/894428): Remove and use the embedded test server instead of data urls.
        ChromeFeatureList.AUTOFILL_ALLOW_NON_HTTP_ACTIVATION})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManualFillingIntegrationTest {
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
    public void testAccessoryIsAvailable() throws InterruptedException {
        mHelper.loadTestPage(false);

        assertNotNull("Controller for Manual filling should be available.",
                mActivityTestRule.getActivity().getManualFillingController());
        assertNotNull("Keyboard accessory should have an instance.",
                mActivityTestRule.getActivity()
                        .getManualFillingController()
                        .getKeyboardAccessory());
        assertNotNull("Accessory Sheet should have an instance.",
                mActivityTestRule.getActivity()
                        .getManualFillingController()
                        .getMediatorForTesting()
                        .getAccessorySheet());
    }

    @Test
    @SmallTest
    public void testKeyboardAccessoryHiddenUntilKeyboardShows()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        onView(withId(R.id.keyboard_accessory)).check(doesNotExist());
        mHelper.clickPasswordField();
        mHelper.sendCredentials(createTestCredentials());
        mHelper.waitForKeyboard();

        // Check that ONLY the accessory is there but the sheet is still hidden.
        whenDisplayed(withId(R.id.keyboard_accessory));
        onView(withChild(withId(R.id.keyboard_accessory_sheet))).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testKeyboardAccessoryDisappearsWithKeyboard()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.sendCredentials(createTestCredentials());
        mHelper.waitForKeyboard();
        whenDisplayed(withId(R.id.keyboard_accessory));

        // Dismiss the keyboard to hide the accessory again.
        mHelper.clickSubmit();
        mHelper.waitForKeyboardToDisappear();
    }

    @Test
    @SmallTest
    public void testAccessorySheetHiddenUntilManuallyTriggered()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Check that ONLY the accessory is there but the sheet is still hidden.
        whenDisplayed(withId(R.id.keyboard_accessory));
        onView(withChild(withId(R.id.keyboard_accessory_sheet))).check(doesNotExist());

        // Trigger the sheet and wait for it to open and the keyboard to disappear.
        onView(allOf(isDisplayed(), isAssignableFrom(KeyboardAccessoryTabLayoutView.class)))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));
    }

    @Test
    @SmallTest
    public void testAccessorySheetHiddenWhenRefocusingField()
            throws InterruptedException, TimeoutException {
        AtomicReference<ViewGroup.MarginLayoutParams> accessoryMargins = new AtomicReference<>();
        AtomicReference<View> accessorySheetView = new AtomicReference<>();
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Check that ONLY the accessory is there but the sheet is still hidden.
        whenDisplayed(withId(R.id.keyboard_accessory)).check((view, e) -> {
            accessoryMargins.set((ViewGroup.MarginLayoutParams) view.getLayoutParams());
            assertThat(accessoryMargins.get().bottomMargin, is(0)); // Attached to keyboard.
        });
        onView(withChild(withId(R.id.keyboard_accessory_sheet))).check(doesNotExist());

        // Trigger the sheet and wait for it to open and the keyboard to disappear.
        onView(allOf(isDisplayed(), isAssignableFrom(KeyboardAccessoryTabLayoutView.class)))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet))).check((view, e) -> {
            accessorySheetView.set(view);
        });
        // The accessory bar is now pushed up by the accessory.
        CriteriaHelper.pollUiThread(() -> {
            return accessoryMargins.get().bottomMargin == accessorySheetView.get().getHeight();
        });

        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        mHelper.waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
        CriteriaHelper.pollUiThread(() -> accessoryMargins.get().bottomMargin == 0);
    }

    @Test
    @SmallTest
    public void testHidingSheetBringsBackKeyboard() throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(allOf(isDisplayed(), isAssignableFrom(KeyboardAccessoryTabLayoutView.class)))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));

        // Click the tab again to hide the sheet and show the keyboard.
        onView(allOf(isDisplayed(), isAssignableFrom(KeyboardAccessoryTabLayoutView.class)))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboard();
        onView(withId(R.id.keyboard_accessory)).check(matches(isDisplayed()));
        mHelper.waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
    }

    @Test
    @SmallTest
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
    public void testOpeningSheetDismissesAutofill()
            throws InterruptedException, TimeoutException, ExecutionException {
        mHelper.loadTestPage(false);
        new AutofillTestHelper().setProfile(new PersonalDataManager.AutofillProfile("",
                "https://www.example.com/", "Alan Turing", "", "Street Ave 4", "", "Capitaltown",
                "", "80666", "", "Disneyland", "1", "a.turing@enigma.com", "DE"));

        // Focus the field to bring up the autofill popup. We force a accessory here because the
        // autofill popup doesn't trigger on password fields.
        mHelper.clickEmailField(true);
        mHelper.waitForKeyboard();

        DropdownPopupWindowInterface popup = mHelper.waitForAutofillPopup("a.tu");

        assertThat(popup.isShowing(), is(true));

        // Click the tab to show the sheet and hide keyboard and popup.
        whenDisplayed(allOf(isDisplayed(), isAssignableFrom(KeyboardAccessoryTabLayoutView.class)))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));

        assertThat(popup.isShowing(), is(false));
    }

    @Test
    @SmallTest
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
    public void testSelectingNonPasswordInputDismissesAccessory()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the password field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        whenDisplayed(allOf(isDisplayed(), isAssignableFrom(KeyboardAccessoryTabLayoutView.class)));

        // Clicking the email field hides the accessory again.
        mHelper.clickEmailField(false);
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/919988")
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testInvokingTabSwitcherHidesAccessory()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(allOf(isDisplayed(), isAssignableFrom(KeyboardAccessoryTabLayoutView.class)))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getLayoutManager().showOverview(false); });
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getLayoutManager().hideOverview(false); });

        mHelper.waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
    }

    @Test
    @SmallTest
    public void testResumingTheAppDismissesAllInputMethods()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(allOf(isDisplayed(), isAssignableFrom(KeyboardAccessoryTabLayoutView.class)))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));

        // Simulate backgrounding the main activity.
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().onPauseWithNative(); });

        // This should completely dismiss any input method.
        mHelper.waitForKeyboardToDisappear();
        mHelper.waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory));

        // Simulate foregrounding the main activity.
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().onResumeWithNative(); });

        // Clicking the field should bring the accessory back up.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(allOf(isDisplayed(), isAssignableFrom(KeyboardAccessoryTabLayoutView.class)))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));
    }

    @Test
    @SmallTest
    public void testPressingBackButtonHidesAccessorySheet()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(allOf(isDisplayed(), isAssignableFrom(KeyboardAccessoryTabLayoutView.class)))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));

        Espresso.pressBack();

        mHelper.waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory));
    }

    @Test
    @SmallTest
    public void testInfobarStaysHiddenWhileChangingFieldsWithOpenKeybaord()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        InfoBarTestAnimationListener listener = new InfoBarTestAnimationListener();
        mActivityTestRule.getInfoBarContainer().addAnimationListener(listener);
        final String kInfoBarText = "SomeInfoBar";
        ThreadUtils.runOnUiThread(() -> {
            SimpleConfirmInfoBarBuilder.create(mActivityTestRule.getActivity().getActivityTab(),
                    InfoBarIdentifier.DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID, kInfoBarText,
                    false);
        });
        listener.addInfoBarAnimationFinished("InfoBar not added.");

        mHelper.sendCredentials(createTestCredentials());
        whenDisplayed(withText(kInfoBarText));

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(not(View.VISIBLE)));

        // Clicking another field hides the accessory, but the InfoBar should remain invisible.
        mHelper.clickEmailField(false);
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(not(View.VISIBLE)));

        // Close the keyboard to bring back the InfoBar.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getKeyboardDelegate().hideKeyboard(
                    mActivityTestRule.getActivity().getCurrentFocus());
            mActivityTestRule.getInfoBarContainer().requestLayout();
        });

        mHelper.waitForKeyboardToDisappear();
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory));

        whenDisplayed(withText(kInfoBarText));
    }

    @Test
    @SmallTest
    public void testInfobarStaysHiddenWhenOpeningSheet()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        InfoBarTestAnimationListener listener = new InfoBarTestAnimationListener();
        mActivityTestRule.getInfoBarContainer().addAnimationListener(listener);
        final String kInfoBarText = "SomeInfoBar";
        ThreadUtils.runOnUiThread(() -> {
            SimpleConfirmInfoBarBuilder.create(mActivityTestRule.getActivity().getActivityTab(),
                    InfoBarIdentifier.DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID, kInfoBarText,
                    false);
        });
        listener.addInfoBarAnimationFinished("InfoBar not added.");

        mHelper.sendCredentials(createTestCredentials());
        whenDisplayed(withText(kInfoBarText));

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(not(View.VISIBLE)));

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(allOf(isDisplayed(), isAssignableFrom(KeyboardAccessoryTabLayoutView.class)))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(not(View.VISIBLE)));

        // Reopen the keyboard, then close it.
        whenDisplayed(allOf(isDisplayed(), isAssignableFrom(KeyboardAccessoryTabLayoutView.class)))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboard();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getKeyboardDelegate().hideKeyboard(
                    mActivityTestRule.getActivity().getCurrentFocus());
            mActivityTestRule.getInfoBarContainer().requestLayout();
        });

        mHelper.waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory));

        whenDisplayed(withText(kInfoBarText));
    }

    @Test
    @SmallTest
    public void testMovesUpSnackbar() throws InterruptedException, TimeoutException {
        final String kSnackbarText = "snackbar";

        mHelper.loadTestPage(false);

        // Create a simple, persistent snackbar and verify it's displayed.
        SnackbarManager manager = mActivityTestRule.getActivity().getSnackbarManager();
        ThreadUtils.runOnUiThread(
                ()
                        -> manager.showSnackbar(Snackbar.make(kSnackbarText,
                                new SnackbarManager.SnackbarController() {},
                                Snackbar.TYPE_PERSISTENT, Snackbar.UMA_TEST_SNACKBAR)));
        CriteriaHelper.pollUiThread(manager::isShowing);
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getWindowAndroid()::haveAnimationsEnded);

        // Click in a field to open keyboard and accessory -- this shouldn't hide the snackbar.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        whenDisplayed(withId(R.id.keyboard_accessory));
        onView(withText(kSnackbarText)).check(matches(isCompletelyDisplayed()));

        // Open a keyboard accessory sheet -- this also shouldn't hide the snackbar.
        whenDisplayed(allOf(isDisplayed(), isAssignableFrom(KeyboardAccessoryTabLayoutView.class)))
                .perform(selectTabAtPosition(0));
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));
        onView(withText(kSnackbarText)).check(matches(isCompletelyDisplayed()));

        // Click into the email field to dismiss the keyboard accessory.
        mHelper.clickEmailField(false);
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory));
        onView(withText(kSnackbarText)).check(matches(isCompletelyDisplayed()));
    }

    @Test
    @SmallTest
    public void testInfobarReopensOnPressingBack() throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        InfoBarTestAnimationListener listener = new InfoBarTestAnimationListener();
        mActivityTestRule.getInfoBarContainer().addAnimationListener(listener);
        final String kInfoBarText = "SomeInfoBar";
        ThreadUtils.runOnUiThread(() -> {
            SimpleConfirmInfoBarBuilder.create(mActivityTestRule.getActivity().getActivityTab(),
                    InfoBarIdentifier.DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID, kInfoBarText,
                    false);
        });
        listener.addInfoBarAnimationFinished("InfoBar not added.");

        mHelper.sendCredentials(createTestCredentials());
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(View.VISIBLE));

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        whenDisplayed(allOf(isDisplayed(), isAssignableFrom(KeyboardAccessoryTabLayoutView.class)))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(not(View.VISIBLE)));

        // Close the accessory using the back button. The Infobar should reappear.
        Espresso.pressBack();

        mHelper.waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory));

        whenDisplayed(withText(kInfoBarText));
    }
}
