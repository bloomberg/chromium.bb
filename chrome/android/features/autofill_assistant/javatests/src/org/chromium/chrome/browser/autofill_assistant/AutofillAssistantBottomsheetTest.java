// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.actionWithAssertions;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.scrollTo;
import static android.support.test.espresso.action.ViewActions.typeText;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withTagValue;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.autofill_assistant.AssistantTagsForTesting.RECYCLER_VIEW_TAG;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getAbsoluteBoundingRect;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilKeyboardMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.PeekMode.HANDLE;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.PeekMode.HANDLE_HEADER;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.PeekMode.HANDLE_HEADER_CAROUSELS;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.ViewportResizing.NO_RESIZE;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.ViewportResizing.RESIZE_LAYOUT_VIEWPORT;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.ViewportResizing.RESIZE_VISUAL_VIEWPORT;

import android.graphics.Rect;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.ViewAction;
import android.support.test.espresso.action.GeneralLocation;
import android.support.test.espresso.action.GeneralSwipeAction;
import android.support.test.espresso.action.Press;
import android.support.test.espresso.action.Swipe;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipIcon;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientSettingsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.PeekMode;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.ViewportResizing;
import org.chromium.chrome.browser.autofill_assistant.proto.DetailsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementReferenceProto;
import org.chromium.chrome.browser.autofill_assistant.proto.FocusElementProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowDetailsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputProto.InputType;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputSectionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UserFormSectionProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests autofill assistant bottomsheet.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantBottomsheetTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "bottomsheet_behaviour_target_website.html";

    @Before
    public void setUp() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(),
                mTestRule.getTestServer().getURL(TEST_PAGE)));
        mTestRule.getActivity().getScrim().disableAnimationForTesting(true);
    }

    private AutofillAssistantTestScript makeScriptWithActionArray(
            ArrayList<ActionProto> actionsList) {
        return new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("bottomsheet_behaviour_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Bottomsheet behaviour")))
                        .build(),
                actionsList);
    }

    private AutofillAssistantTestScript makeScript(
            ViewportResizing resizing, PeekMode peekMode, boolean withDetails) {
        ArrayList<ActionProto> list = new ArrayList<>();
        // Prompt.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Hello world!")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.DONE_ACTION)
                                                            .setText("Focus element"))))
                         .build());
        // Set viewport resizing and peek mode.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setConfigureBottomSheet(ConfigureBottomSheetProto.newBuilder()
                                                          .setViewportResizing(resizing)
                                                          .setPeekMode(peekMode))
                         .build());
        // Focus on the bottom element.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setFocusElement(FocusElementProto.newBuilder().setElement(
                                 ElementReferenceProto.newBuilder().addSelectors("p.bottom")))
                         .build());
        if (withDetails) {
            // ShowDetails.
            list.add((ActionProto) ActionProto.newBuilder()
                             .setShowDetails(ShowDetailsProto.newBuilder().setDetails(
                                     DetailsProto.newBuilder()
                                             .setTitle("Details title")
                                             .setShowImagePlaceholder(true)))
                             .build());
        }
        // Add "Done" button.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 Choice.newBuilder().setChip(ChipProto.newBuilder()
                                                                     .setType(ChipType.DONE_ACTION)
                                                                     .setText("Done"))))
                         .build());

        return makeScriptWithActionArray(list);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1059442")
    public void testNoResize() {
        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(makeScript(NO_RESIZE, HANDLE, false)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Focus element"), isCompletelyDisplayed());
        onView(withText("Focus element")).perform(click());
        checkElementIsCoveredByBottomsheet("bottom", true);
        onView(withId(R.id.swipe_indicator)).perform(swipeDownToMinimize());
        waitUntilViewMatchesCondition(withText("Hello world!"), not(isDisplayed()));
        // Since no resizing of the viewport happens in this mode, the element is partially covered
        // even when the bottomsheet is minimized
        checkElementIsCoveredByBottomsheet("bottom", true);
        onView(withText("Done")).check(matches(not(isDisplayed())));
        onView(withId(R.id.swipe_indicator)).perform(swipeUpToExpand());
        checkElementIsCoveredByBottomsheet("bottom", true);
        waitUntilViewMatchesCondition(withText("Hello world!"), isDisplayed());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1059442")
    public void testResizeLayoutViewport() {
        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(makeScript(RESIZE_LAYOUT_VIEWPORT, HANDLE, false)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Focus element"), isCompletelyDisplayed());
        onView(withText("Focus element")).perform(click());
        // The viewport should be resized so that the bottom element is not covered by the bottom
        // sheet.
        checkElementIsCoveredByBottomsheet("bottom", false);
        onView(withId(R.id.swipe_indicator)).perform(swipeDownToMinimize());
        // Minimizing the bottomsheet should completely uncover the bottom element.
        waitUntilViewMatchesCondition(withText("Hello world!"), not(isDisplayed()));
        checkElementIsCoveredByBottomsheet("bottom", false);
        onView(withText("Done")).check(matches(not(isDisplayed())));
        onView(withId(R.id.swipe_indicator)).perform(swipeUpToExpand());
        checkElementIsCoveredByBottomsheet("bottom", true);
        waitUntilViewMatchesCondition(withText("Hello world!"), isDisplayed());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1059442")
    public void testResizeVisualViewport() {
        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(makeScript(RESIZE_VISUAL_VIEWPORT, HANDLE, false)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Focus element"), isCompletelyDisplayed());
        onView(withText("Focus element")).perform(click());
        // The viewport should be resized so that the bottom element is not covered by the bottom
        // sheet.
        checkElementIsCoveredByBottomsheet("bottom", false);
        onView(withId(R.id.swipe_indicator)).perform(swipeDownToMinimize());
        waitUntilViewMatchesCondition(withText("Hello world!"), not(isDisplayed()));
        checkElementIsCoveredByBottomsheet("bottom", false);
        onView(withText("Done")).check(matches(not(isDisplayed())));
        onView(withId(R.id.swipe_indicator)).perform(swipeUpToExpand());
        checkElementIsCoveredByBottomsheet("bottom", true);
        waitUntilViewMatchesCondition(withText("Hello world!"), isDisplayed());
    }

    @Test
    @MediumTest
    public void testHandleHeader() {
        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(makeScript(RESIZE_LAYOUT_VIEWPORT, HANDLE_HEADER, true)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Focus element"), isCompletelyDisplayed());
        onView(withText("Focus element")).perform(click());
        waitUntilViewMatchesCondition(withText("Details title"), isCompletelyDisplayed());
        checkElementIsCoveredByBottomsheet("bottom", true);
        onView(withId(R.id.swipe_indicator)).perform(swipeDownToMinimize());
        checkElementIsCoveredByBottomsheet("bottom", false);
        // The header should be visible even when minimized
        onView(withText("Hello world!")).check(matches(isCompletelyDisplayed()));
        onView(withText("Details title")).check(matches(not(isDisplayed())));
        onView(withText("Done")).check(matches(not(isDisplayed())));
        onView(withId(R.id.swipe_indicator)).perform(swipeUpToExpand());
        checkElementIsCoveredByBottomsheet("bottom", true);
        onView(withText("Details title")).check(matches(isCompletelyDisplayed()));
    }

    @Test
    @MediumTest
    public void testHandleHeaderCarousels() {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(
                        makeScript(RESIZE_LAYOUT_VIEWPORT, HANDLE_HEADER_CAROUSELS, true)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Focus element"), isCompletelyDisplayed());
        onView(withText("Focus element")).perform(click());
        waitUntilViewMatchesCondition(withText("Details title"), isCompletelyDisplayed());
        checkElementIsCoveredByBottomsheet("bottom", true);
        onView(withId(R.id.swipe_indicator)).perform(swipeDownToMinimize());
        checkElementIsCoveredByBottomsheet("bottom", false);
        // The header should be visible even when minimized
        onView(withText("Hello world!")).check(matches(isCompletelyDisplayed()));
        // The button gets initially hidden while swiping down but should reappear shortly after.
        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        onView(withText("Details title")).check(matches(not(isDisplayed())));
        onView(withId(R.id.swipe_indicator)).perform(swipeUpToExpand());
        checkElementIsCoveredByBottomsheet("bottom", true);
        onView(withText("Details title")).check(matches(isCompletelyDisplayed()));
    }

    @Test
    @MediumTest
    public void testBottomSheetDoesNotObstructNavBar() {
        // Create enough additional sections to fill up more than the height of the screen.
        List<UserFormSectionProto> additionalSections = new ArrayList<>();
        for (int i = 0; i < 20; ++i) {
            additionalSections.add(
                    (UserFormSectionProto) UserFormSectionProto.newBuilder()
                            .setTextInputSection(TextInputSectionProto.newBuilder().addInputFields(
                                    TextInputProto.newBuilder()
                                            .setHint("Text input " + i)
                                            .setClientMemoryKey("input_" + i)
                                            .setInputType(TextInputProto.InputType.INPUT_TEXT)))
                            .setTitle("Title " + i)
                            .build());
        }

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .addAllAdditionalAppendedSections(additionalSections)
                                         .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("bottomsheet_behaviour_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());

        onView(withId(R.id.control_container)).check(matches(isCompletelyDisplayed()));
        onView(withText("Title 0")).perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Text input 0"), isCompletelyDisplayed());
        // Typing text will show the soft keyboard, leading to resize of the Chrome window.
        onView(withContentDescription("Text input 0")).perform(typeText("Hello World!"));
        onView(withId(R.id.control_container)).check(matches(isCompletelyDisplayed()));
        onView(allOf(withContentDescription("Close"), isDisplayed()))
                .check(matches(isCompletelyDisplayed()));
        // Closing the soft keyboard will restore the window size.
        Espresso.closeSoftKeyboard();
        onView(withContentDescription("Text input 0")).check(matches(isDisplayed()));
        onView(withId(R.id.control_container)).check(matches(isCompletelyDisplayed()));
        onView(withText("Continue")).check(matches(isCompletelyDisplayed()));

        // Scroll down.
        onView(withText("Title 19")).check(matches(not(isDisplayed())));
        onView(withText("Title 19")).perform(scrollTo()).check(matches(isDisplayed()));

        // Scroll up.
        onView(withText("Title 0")).check(matches(not(isDisplayed())));
        onView(withText("Title 0")).perform(scrollTo()).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testCancelSnackbarUndo() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(Choice.newBuilder().setChip(
                                 ChipProto.newBuilder()
                                         .setType(ChipType.CANCEL_ACTION)
                                         .setIcon(ChipIcon.ICON_CLEAR)
                                         .setText("Cancel"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("bottomsheet_behaviour_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
        waitUntilViewMatchesCondition(withText("Cancel"), isCompletelyDisplayed());

        onView(withText("Cancel")).perform(click());
        waitUntilViewMatchesCondition(withText(R.string.undo), isCompletelyDisplayed());
        onView(withText("Cancel")).check(doesNotExist());
        onView(withText(R.string.undo)).perform(click());
        waitUntilViewMatchesCondition(withText("Cancel"), isDisplayed());
    }

    @Test
    @MediumTest
    public void testCancelSnackbarTimeout() {
        ClientSettingsProto clientSettings = (ClientSettingsProto) ClientSettingsProto.newBuilder()
                                                     .setCancelDelayMs(2000)
                                                     .build();
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(Choice.newBuilder().setChip(
                                 ChipProto.newBuilder()
                                         .setType(ChipType.CANCEL_ACTION)
                                         .setIcon(ChipIcon.ICON_CLEAR)
                                         .setText("Cancel"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("bottomsheet_behaviour_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script), clientSettings);
        startAutofillAssistant(mTestRule.getActivity(), testService);
        waitUntilViewMatchesCondition(withText("Cancel"), isCompletelyDisplayed());

        onView(withText("Cancel")).perform(click());
        waitUntilViewMatchesCondition(withText(R.string.undo), isCompletelyDisplayed());
        onView(withText("Cancel")).check(doesNotExist());
        waitUntilViewAssertionTrue(withText(R.string.undo), doesNotExist(), 3000L);
        onView(withId(R.id.autofill_assistant)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testBottomSheetAutoCollapseAndExpand() {
        ArrayList<ActionProto> list = new ArrayList<>();
        // Prompt.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Hello world!")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.DONE_ACTION)
                                                            .setText("Focus element"))))
                         .build());
        // Focus on the bottom element.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setFocusElement(FocusElementProto.newBuilder().setElement(
                                 ElementReferenceProto.newBuilder().addSelectors("p.bottom")))
                         .build());
        // Set handle and header peek mode and auto collapse to that state.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setConfigureBottomSheet(ConfigureBottomSheetProto.newBuilder()
                                                          .setViewportResizing(NO_RESIZE)
                                                          .setPeekMode(HANDLE_HEADER)
                                                          .setCollapse(true))
                         .build());
        // Add sticky "Next" button. Disable auto expanding the sheet for prompt actions.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.DONE_ACTION)
                                                            .setSticky(true)
                                                            .setText("Sticky next")))
                                            .setDisableForceExpandSheet(true))
                         .build());
        // Expand the sheet.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setConfigureBottomSheet(ConfigureBottomSheetProto.newBuilder()
                                                          .setViewportResizing(NO_RESIZE)
                                                          .setExpand(true))
                         .build());
        // Add "Done" button.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 Choice.newBuilder().setChip(ChipProto.newBuilder()
                                                                     .setType(ChipType.DONE_ACTION)
                                                                     .setText("Done"))))
                         .build());

        AutofillAssistantTestScript script = makeScriptWithActionArray(list);
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Focus element"), isCompletelyDisplayed());
        onView(withText("Focus element")).perform(click());

        // Check that the sheet is in peek state and has a sticky button. There is
        // a second button still in the hidden carousel.
        waitUntilViewMatchesCondition(
                allOf(withText("Sticky next"), isDescendantOfA(withId(R.id.header))),
                isCompletelyDisplayed());
        onView(allOf(withText("Sticky next"), isDescendantOfA(withTagValue(is(RECYCLER_VIEW_TAG)))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withText("Sticky next"), isDescendantOfA(withId(R.id.header))))
                .perform(click());

        // Check that the sheet is now expanded and the done button is part of the recycler view,
        // not the header.
        waitUntilViewMatchesCondition(
                allOf(withText("Done"), isDescendantOfA(withTagValue(is(RECYCLER_VIEW_TAG)))),
                isCompletelyDisplayed());
    }

    /**
     * When the keyboard is shown, the continue button becomes invisible.
     */
    @Test
    @DisabledTest(message = "Test is flaky, see crbug.com/1054058")
    @MediumTest
    public void testOpeningKeyboardMakesContinueChipInvisible() {
        ArrayList<ActionProto> list = new ArrayList<>();
        UserFormSectionProto userFormSectionProto =
                UserFormSectionProto.newBuilder()
                        .setTitle("User form")
                        .setTextInputSection(
                                TextInputSectionProto.newBuilder()
                                        .addInputFields(TextInputProto.newBuilder()
                                                                .setHint("Field 1")
                                                                .setInputType(InputType.INPUT_TEXT)
                                                                .setClientMemoryKey("field_1"))
                                        .addInputFields(TextInputProto.newBuilder()
                                                                .setHint("Field 2")
                                                                .setInputType(InputType.INPUT_TEXT)
                                                                .setClientMemoryKey("field_2")))
                        .build();

        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setRequestTermsAndConditions(false)
                                         .addAdditionalPrependedSections(userFormSectionProto))
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

        waitUntilViewMatchesCondition(withText("User form"), isDisplayed());
        onView(withText("User form")).perform(click());
        waitUntilViewMatchesCondition(withText("Field 1"), isDisplayed());
        onView(withContentDescription("Continue")).check(matches(isDisplayed()));
        onView(withText("Field 1")).perform(click());
        waitUntilKeyboardMatchesCondition(mTestRule, true);
        onView(withContentDescription("Continue")).check(matches(not(isDisplayed())));
        onView(allOf(withContentDescription("Close"), isDisplayed())).perform(click());
        waitUntilKeyboardMatchesCondition(mTestRule, false);
        onView(withContentDescription("Continue")).check(matches(isDisplayed()));
    }

    private ViewAction swipeDownToMinimize() {
        return actionWithAssertions(
                new GeneralSwipeAction(Swipe.FAST, GeneralLocation.CENTER, view -> {
                    float[] coordinates = GeneralLocation.CENTER.calculateCoordinates(view);
                    coordinates[1] =
                            view.getContext().getResources().getDisplayMetrics().heightPixels;
                    return coordinates;
                }, Press.FINGER));
    }

    private ViewAction swipeUpToExpand() {
        return actionWithAssertions(
                new GeneralSwipeAction(Swipe.FAST, GeneralLocation.CENTER, view -> {
                    float[] coordinates = GeneralLocation.CENTER.calculateCoordinates(view);
                    coordinates[1] = 0;
                    return coordinates;
                }, Press.FINGER));
    }

    private void checkElementIsCoveredByBottomsheet(String elementId, boolean shouldBeCovered) {
        CriteriaHelper.pollInstrumentationThread(new Criteria("Timeout while waiting for element '"
                + elementId + "' to become " + (shouldBeCovered ? "covered" : "not covered")
                + " by the bottomsheet") {
            @Override
            public boolean isSatisfied() {
                try {
                    float y = GeneralLocation.TOP_CENTER.calculateCoordinates(
                            mTestRule.getActivity().findViewById(
                                    R.id.autofill_assistant_bottom_sheet_toolbar))[1];
                    Rect el = getAbsoluteBoundingRect(mTestRule, elementId);
                    return el.bottom > y == shouldBeCovered;
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        });
    }
}
