// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onData;
import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.clearText;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.scrollTo;
import static android.support.test.espresso.action.ViewActions.typeText;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.RootMatchers.withDecorView;
import static android.support.test.espresso.matcher.ViewMatchers.hasDescendant;
import static android.support.test.espresso.matcher.ViewMatchers.isChecked;
import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isEnabled;
import static android.support.test.espresso.matcher.ViewMatchers.withChild;
import static android.support.test.espresso.matcher.ViewMatchers.withClassName;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anything;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getElementValue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.isNextAfterSibling;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.filters.MediumTest;
import android.widget.RadioButton;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ContactDetailsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementReferenceProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UseAddressProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UseAddressProto.RequiredField;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;

/**
 * Tests autofill assistant's interaction with the PersonalDataManager.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantPersonalDataManagerTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "form_target_website.html";

    private AutofillAssistantCollectUserDataTestHelper mHelper;

    private WebContents getWebContents() {
        return mTestRule.getWebContents();
    }

    @Before
    public void setUp() throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(),
                mTestRule.getTestServer().getURL(TEST_PAGE)));
        mHelper = new AutofillAssistantCollectUserDataTestHelper();
    }

    /**
     * Add a contact with Autofill Assistant UI and fill it into the form.
     */
    @Test
    @MediumTest
    public void testCreateAndEnterProfile() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setUseAddress(
                                 UseAddressProto.newBuilder()
                                         .setName("contact")
                                         .setFormFieldElement(
                                                 ElementReferenceProto.newBuilder().addSelectors(
                                                         "#profile_name"))
                                         .addRequiredFields(
                                                 RequiredField.newBuilder()
                                                         .setValueExpression("7")
                                                         .setElement(
                                                                 ElementReferenceProto.newBuilder()
                                                                         .addSelectors(
                                                                                 "#profile_name")))
                                         .addRequiredFields(
                                                 RequiredField.newBuilder()
                                                         .setValueExpression("9")
                                                         .setElement(
                                                                 ElementReferenceProto.newBuilder()
                                                                         .addSelectors("#email"))))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Address")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")),
                isCompletelyDisplayed());
        onView(allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(typeText("John Doe"));
        waitUntilViewMatchesCondition(
                withContentDescription("Email*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Email*")).perform(typeText("johndoe@google.com"));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button)).perform(click());
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        onView(withId(R.id.contact_summary))
                .check(matches(allOf(withText("johndoe@google.com"), isDisplayed())));
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue(getWebContents(), "profile_name"), is("John Doe"));
        assertThat(getElementValue(getWebContents(), "email"), is("johndoe@google.com"));
    }

    /**
     * Add a contact with Autofill Assistant UI, then edit the profile multiple times (see
     * b/153139772).
     */
    @Test
    @MediumTest
    public void testCreateAndEditProfileMultipleTimes() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Address")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")),
                isCompletelyDisplayed());
        onView(allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(typeText("John Doe"));
        waitUntilViewMatchesCondition(
                withContentDescription("Email*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Email*")).perform(typeText("doe@google.com"));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button)).perform(click());
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());

        // First edit: no changes.
        onView(withText("Contact info")).perform(click());
        onView(withContentDescription("Edit contact info")).perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button)).perform(click());

        // Second edit: change name from John Doe to Jane Doe.
        onView(withContentDescription("Edit contact info")).perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(clearText(), typeText("Jane Doe"));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button)).perform(click());

        // There used to be a bug where consecutive edits of the same profile would create a
        // duplicate profile, which would break the following checks.
        onView(withText(containsString("John Doe"))).check(doesNotExist());
        onView(withText(containsString("Jane Doe"))).check(matches(isDisplayed()));
    }

    /**
     * Catch the insert of a profile added outside of the Autofill Assistant, e.g. with the Chrome
     * settings UI, and fill it into the form.
     */
    @Test
    @MediumTest
    public void testExternalAddAndEnterProfile() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setUseAddress(
                                UseAddressProto.newBuilder().setName("contact").setFormFieldElement(
                                        ElementReferenceProto.newBuilder().addSelectors(
                                                "#profile_name")))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Address")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")),
                isCompletelyDisplayed());
        mHelper.addDummyProfile("John Doe", "johndoe@google.com");
        waitUntilViewMatchesCondition(
                withId(R.id.contact_summary), allOf(withText("johndoe@google.com"), isDisplayed()));
        waitUntilViewMatchesCondition(withContentDescription("Continue"), isEnabled());
        onView(withContentDescription("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue(getWebContents(), "profile_name"), is("John Doe"));
        assertThat(getElementValue(getWebContents(), "email"), is("johndoe@google.com"));
    }

    /**
     * A new profile added outside of the Autofill Assistant, e.g. with the Chrome settings UI,
     * should not overwrite the current selection.
     */
    @Test
    @MediumTest
    public void testExternalAddNewProfile() throws Exception {
        mHelper.addDummyProfile("John Doe", "johndoe@google.com");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setUseAddress(
                                UseAddressProto.newBuilder().setName("contact").setFormFieldElement(
                                        ElementReferenceProto.newBuilder().addSelectors(
                                                "#profile_name")))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Address")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                withId(R.id.contact_summary), allOf(withText("johndoe@google.com"), isDisplayed()));
        onView(withText("Continue")).check(matches(isEnabled()));
        // Add new entry that is not supposed to be selected.
        mHelper.addDummyProfile("Adam West", "adamwest@google.com");
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(withText(containsString("Adam West")), isDisplayed());
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(
                withId(R.id.contact_summary), allOf(withText("johndoe@google.com"), isDisplayed()));
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        // Make sure it's not Adam West that was selected.
        assertThat(getElementValue(getWebContents(), "profile_name"), is("John Doe"));
        assertThat(getElementValue(getWebContents(), "email"), is("johndoe@google.com"));
    }

    /**
     * An existing profile deleted outside of the Autofill Assistant, e.g. with the Chrome settings
     * UI, should be removed from the current list.
     */
    @Test
    @MediumTest
    public void testExternalDeleteProfile() throws Exception {
        String profileIdA = mHelper.addDummyProfile("Adam Doe", "adamdoe@google.com");
        String profileIdB = mHelper.addDummyProfile("Berta Doe", "bertadoe@google.com");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Address")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                withId(R.id.contact_summary), allOf(withText("adamdoe@google.com"), isDisplayed()));
        // Delete first profile, expect second to be selected.
        mHelper.deleteProfile(profileIdA);
        waitUntilViewMatchesCondition(withId(R.id.contact_summary),
                allOf(withText("bertadoe@google.com"), isDisplayed()));
        // Delete second profile, expect nothing to be selected.
        mHelper.deleteProfile(profileIdB);
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")),
                isCompletelyDisplayed());
    }

    /**
     * Editing the currently selected contact in the Assistant Autofill UI should keep selection.
     */
    @Test
    @MediumTest
    public void testEditOfSelectedProfile() throws Exception {
        mHelper.addDummyProfile("Adam West", "adamwest@google.com");
        mHelper.addDummyProfile("John Doe", "johndoe@google.com");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setUseAddress(
                                UseAddressProto.newBuilder().setName("contact").setFormFieldElement(
                                        ElementReferenceProto.newBuilder().addSelectors(
                                                "#profile_name")))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Address")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(withId(R.id.contact_summary),
                allOf(withText("adamwest@google.com"), isDisplayed()));
        onView(withText("Continue")).check(matches(isEnabled()));
        // Select John Doe.
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(withText(containsString("John Doe")), isDisplayed());
        onView(withText(containsString("John Doe"))).perform(click());
        waitUntilViewMatchesCondition(
                withId(R.id.contact_summary), allOf(withText("johndoe@google.com"), isDisplayed()));
        // Edit John Doe to Jane Doe (does not collapse the list after editing).
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(withText(containsString("John Doe")), isDisplayed());
        onView(allOf(withContentDescription("Edit contact info"),
                       isNextAfterSibling(hasDescendant(withText(containsString("John Doe"))))))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(clearText()).perform(typeText("Jane Doe"));
        waitUntilViewMatchesCondition(
                withContentDescription("Email*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Email*"))
                .perform(clearText())
                .perform(typeText("janedoe@google.com"));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button)).perform(click());
        waitUntilViewMatchesCondition(withText("Contact info"), isDisplayed());
        // Continue.
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        // Make sure it's now Jane Doe.
        assertThat(getElementValue(getWebContents(), "profile_name"), is("Jane Doe"));
        assertThat(getElementValue(getWebContents(), "email"), is("janedoe@google.com"));
    }

    /**
     * Add a credit card with Autofill Assistant UI and fill it into the form.
     */
    @Test
    @MediumTest
    public void testCreateAndEnterCard() throws Exception {
        // Add a profile for easier address selection.
        mHelper.addDummyProfile("Adam West", "adamwest@google.com");

        // The Current year is the default selection for expiration year of the credit card.
        int year = Calendar.getInstance().get(Calendar.YEAR);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestPaymentMethod(true)
                                                     .setBillingAddressName("billing_address")
                                                     .setRequestTermsAndConditions(false))
                         .build());
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setUseCard(org.chromium.chrome.browser.autofill_assistant.proto
                                            .UseCreditCardProto.newBuilder()
                                            .setFormFieldElement(
                                                    ElementReferenceProto.newBuilder().addSelectors(
                                                            "#card_number")))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add card")),
                isCompletelyDisplayed());
        onView(allOf(withId(R.id.section_title_add_button_label), withText("Add card")))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Card number*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Card number*")).perform(typeText("4111111111111111"));
        waitUntilViewMatchesCondition(
                withContentDescription("Name on card*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name on card*")).perform(typeText("John Doe"));
        Espresso.closeSoftKeyboard(); // Close keyboard, not to hide the Spinners.
        onView(allOf(withId(org.chromium.chrome.R.id.spinner),
                       withChild(withText(String.valueOf(year)))))
                .perform(click());
        onData(anything())
                .atPosition(2 /* select 2 years in the future, 0 is the current year */)
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());
        onView(allOf(withId(org.chromium.chrome.R.id.spinner), withChild(withText("Select"))))
                .perform(click());
        onData(anything())
                .atPosition(1 /* address of Adam, 0 is SELECT (empty) */)
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button))
                .perform(scrollTo(), click());
        waitUntilViewMatchesCondition(allOf(withId(R.id.credit_card_number),
                                              isDescendantOfA(withId(R.id.payment_method_summary))),
                allOf(withText(containsString("1111")), isDisplayed()));
        onView(withContentDescription("Continue")).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.card_unmask_input), isCompletelyDisplayed());
        onView(withId(R.id.card_unmask_input)).perform(typeText("123"));
        onView(withId(R.id.positive_button)).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue(getWebContents(), "name"), is("John Doe"));
        assertThat(getElementValue(getWebContents(), "card_number"), is("4111111111111111"));
        assertThat(getElementValue(getWebContents(), "cv2_number"), is("123"));
        assertThat(getElementValue(getWebContents(), "exp_month"), is("01"));
        assertThat(getElementValue(getWebContents(), "exp_year"), is(String.valueOf(year + 2)));
    }

    /**
     * Catch the insert of a credit card added outside of the Autofill Assistant, e.g. with the
     * Chrome settings UI, and fill it into the form.
     */
    @Test
    @MediumTest
    public void testExternalAddCreditCard() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestPaymentMethod(true)
                                                     .setBillingAddressName("billing_address")
                                                     .setRequestTermsAndConditions(false))
                         .build());
        // No UseCreditCardAction, that is tested in PaymentTest.
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add card")),
                isCompletelyDisplayed());
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@google.com");
        mHelper.addDummyCreditCard(profileId);
        waitUntilViewMatchesCondition(allOf(withId(R.id.credit_card_number),
                                              isDescendantOfA(withId(R.id.payment_method_summary))),
                allOf(withText(containsString("1111")), isDisplayed()));
        waitUntilViewMatchesCondition(withContentDescription("Continue"), isEnabled());
    }

    /**
     * Catch the insert of a credit card with a billing address missing the postal code added
     * outside of the Autofill Assistant, e.g. with the Chrome settings UI, and fill it into the
     * form.
     */
    @Test
    @MediumTest
    public void testExternalAddCreditCardWithoutBillingZip() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setRequestPaymentMethod(true)
                                         .setBillingAddressName("billing_address")
                                         .setRequireBillingPostalCode(true)
                                         .setBillingPostalCodeMissingText("Missing Billing Code")
                                         .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add card")),
                isCompletelyDisplayed());
        String profileId =
                mHelper.addDummyProfile("John Doe", "johndoe@google.com", /* postcode= */ "");
        mHelper.addDummyCreditCard(profileId);
        waitUntilViewMatchesCondition(allOf(withText("Missing Billing Code"),
                                              isDescendantOfA(withId(R.id.payment_method_summary))),
                isDisplayed());
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
    }

    /**
     * An existing credit card deleted outside of the Autofill Assistant, e.g. with the Chrome
     * settings UI, should be removed from the current list.
     */
    @Test
    @MediumTest
    public void testExternalDeleteCreditCard() throws Exception {
        String profileId;

        profileId = mHelper.addDummyProfile("Adam Doe", "adamdoe@google.com");
        String cardIdA = mHelper.addDummyCreditCard(profileId, "4111111111111111");

        profileId = mHelper.addDummyProfile("Berta Doe", "bertadoe@google.com");
        String cardIdB = mHelper.addDummyCreditCard(profileId, "5555555555554444");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestPaymentMethod(true)
                                                     .setBillingAddressName("billing_address")
                                                     .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(allOf(withId(R.id.credit_card_number),
                                              isDescendantOfA(withId(R.id.payment_method_summary))),
                allOf(withText(containsString("1111")), isDisplayed()));
        onView(withText("Payment method")).perform(click());
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.credit_card_name), withText("Adam Doe")), isDisplayed());
        onView(allOf(withId(R.id.credit_card_name),
                       withParent(allOf(withId(R.id.payment_method_full),
                               isNextAfterSibling(
                                       allOf(instanceOf(RadioButton.class), isChecked()))))))
                .check(matches(withText("Adam Doe")));
        onView(withText("Payment method")).perform(click());
        // Delete first card, expect second card to be selected.
        mHelper.deleteCreditCard(cardIdA);
        waitUntilViewMatchesCondition(allOf(withId(R.id.credit_card_number),
                                              isDescendantOfA(withId(R.id.payment_method_summary))),
                allOf(withText(containsString("4444")), isDisplayed()));
        onView(withText("Payment method")).perform(click());
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.credit_card_name), withText("Berta Doe")), isDisplayed());
        onView(allOf(withId(R.id.credit_card_name),
                       withParent(allOf(withId(R.id.payment_method_full),
                               isNextAfterSibling(
                                       allOf(instanceOf(RadioButton.class), isChecked()))))))
                .check(matches(withText("Berta Doe")));
        onView(withText("Payment method")).perform(click());
        // Delete second card, expect nothing to be selected.
        mHelper.deleteCreditCard(cardIdB);
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add card")),
                isCompletelyDisplayed());
    }

    /**
     * Opens the edit dialog for a server card.
     */
    @Test
    @MediumTest
    public void testEditOfServerCard() throws Exception {
        String profileId = mHelper.addDummyProfile("Adam West", "adamwest@google.com");
        mHelper.addServerCreditCard(mHelper.createDummyCreditCard(
                profileId, "4111111111111111", /* isLocal = */ false));

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestPaymentMethod(true)
                                                     .setBillingAddressName("billing_address")
                                                     .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());

        onView(withText("Payment method")).perform(click());
        waitUntilViewMatchesCondition(withContentDescription("Edit card"), isDisplayed());
        onView(withContentDescription("Edit card")).perform(click());
        waitUntilViewMatchesCondition(withText("Billing address*"), isDisplayed());
        // TODO(b/155624806) edit billing address and fill/check values on test website.
        onView(withId(org.chromium.chrome.R.id.payments_edit_cancel_button)).perform(click());
    }

    private void runScript(AutofillAssistantTestScript script) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
    }
}
