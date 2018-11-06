// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertNull;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.core.AllOf.allOf;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessoryAction.AUTOFILL_SUGGESTION;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessoryAction.GENERATE_PASSWORD_AUTOMATIC;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.ACTIONS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.ACTIVE_TAB;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.BOTTOM_OFFSET_PX;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.TABS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.TAB_SELECTION_CALLBACKS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.VISIBLE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_INVISIBLE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ImageView;

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.modelutil.LazyConstructionPropertyMcp;
import org.chromium.chrome.browser.modelutil.ListModel;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.DeferredViewStubInflationProvider;
import org.chromium.ui.ViewProvider;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * View tests for the keyboard accessory component.
 *
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class KeyboardAccessoryViewTest {
    private PropertyModel mModel;
    private BlockingQueue<KeyboardAccessoryView> mKeyboardAccessoryView;

    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    private KeyboardAccessoryData.Tab createTestTab(String contentDescription) {
        return new KeyboardAccessoryData.Tab(
                mActivityTestRule.getActivity().getResources().getDrawable(
                        android.R.drawable.ic_lock_lock), // Unused.
                contentDescription,
                R.layout.empty_accessory_sheet, // Unused.
                AccessoryTabType.ALL,
                null); // Unused.
    }

    /**
     * Matches a tab with a given content description. Selecting the content description alone will
     * match all icons of the tabs as well.
     * @param description The description to look for.
     * @return Returns a matcher that can be used in |onView| or within other {@link Matcher}s.
     */
    private static Matcher<View> isTabWithDescription(String description) {
        return allOf(withContentDescription(description),
                instanceOf(ImageView.class)); // Match only the image.
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mModel = new PropertyModel
                             .Builder(ACTIONS, TABS, VISIBLE, BOTTOM_OFFSET_PX, ACTIVE_TAB,
                                     TAB_SELECTION_CALLBACKS)
                             .with(TABS, new ListModel<>())
                             .with(ACTIONS, new ListModel<>())
                             .with(VISIBLE, false)
                             .build();
            ViewStub viewStub =
                    mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory_stub);

            mKeyboardAccessoryView = new ArrayBlockingQueue<>(1);
            ViewProvider<KeyboardAccessoryView> provider =
                    new DeferredViewStubInflationProvider<>(viewStub);
            LazyConstructionPropertyMcp.create(
                    mModel, VISIBLE, provider, KeyboardAccessoryViewBinder::bind);
            provider.whenLoaded(mKeyboardAccessoryView::add);
        });
    }

    @Test
    @MediumTest
    public void testAccessoryVisibilityChangedByModel()
            throws ExecutionException, InterruptedException {
        // Initially, there shouldn't be a view yet.
        assertNull(mKeyboardAccessoryView.poll());

        // After setting the visibility to true, the view should exist and be visible.
        ThreadUtils.runOnUiThreadBlocking(() -> { mModel.set(VISIBLE, true); });
        KeyboardAccessoryView view = mKeyboardAccessoryView.take();
        assertEquals(view.getVisibility(), View.VISIBLE);

        // After hiding the view, the view should still exist but be invisible.
        ThreadUtils.runOnUiThreadBlocking(() -> { mModel.set(VISIBLE, false); });
        assertNotEquals(view.getVisibility(), View.VISIBLE);
    }

    @Test
    @MediumTest
    public void testClickableActionAddedWhenChangingModel() {
        final AtomicReference<Boolean> buttonClicked = new AtomicReference<>();
        final KeyboardAccessoryData.Action testAction = new KeyboardAccessoryData.Action(
                "Test Button", GENERATE_PASSWORD_AUTOMATIC, action -> buttonClicked.set(true));

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(ACTIONS).add(testAction);
        });

        onView(isRoot()).check((root, e) -> waitForView((ViewGroup) root, withText("Test Button")));
        onView(withText("Test Button")).perform(click());

        assertTrue(buttonClicked.get());
    }

    @Test
    @MediumTest
    public void testCanAddSingleButtons() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(ACTIONS).set(new KeyboardAccessoryData.Action[] {
                    new KeyboardAccessoryData.Action(
                            "First", GENERATE_PASSWORD_AUTOMATIC, action -> {}),
                    new KeyboardAccessoryData.Action("Second", AUTOFILL_SUGGESTION, action -> {})});
        });

        onView(isRoot()).check((root, e) -> waitForView((ViewGroup) root, withText("First")));
        onView(withText("First")).check(matches(isDisplayed()));
        onView(withText("Second")).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mModel.get(ACTIONS).add(new KeyboardAccessoryData.Action(
                                "Third", GENERATE_PASSWORD_AUTOMATIC, action -> {})));

        onView(isRoot()).check((root, e) -> waitForView((ViewGroup) root, withText("Third")));
        onView(withText("First")).check(matches(isDisplayed()));
        onView(withText("Second")).check(matches(isDisplayed()));
        onView(withText("Third")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testCanRemoveSingleButtons() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(ACTIONS).set(new KeyboardAccessoryData.Action[] {
                    new KeyboardAccessoryData.Action(
                            "First", GENERATE_PASSWORD_AUTOMATIC, action -> {}),
                    new KeyboardAccessoryData.Action(
                            "Second", GENERATE_PASSWORD_AUTOMATIC, action -> {}),
                    new KeyboardAccessoryData.Action(
                            "Third", GENERATE_PASSWORD_AUTOMATIC, action -> {})});
        });

        onView(isRoot()).check((root, e) -> waitForView((ViewGroup) root, withText("First")));
        onView(withText("First")).check(matches(isDisplayed()));
        onView(withText("Second")).check(matches(isDisplayed()));
        onView(withText("Third")).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.get(ACTIONS).remove(mModel.get(ACTIONS).get(1)));

        onView(isRoot()).check((root, e)
                                       -> waitForView((ViewGroup) root, withText("Second"),
                                               VIEW_INVISIBLE | VIEW_GONE | VIEW_NULL));
        onView(withText("First")).check(matches(isDisplayed()));
        onView(withText("Second")).check(doesNotExist());
        onView(withText("Third")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testRemovesTabs() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(TABS).set(new KeyboardAccessoryData.Tab[] {createTestTab("FirstTab"),
                    createTestTab("SecondTab"), createTestTab("ThirdTab")});
        });

        onView(isRoot()).check(
                (root, e) -> waitForView((ViewGroup) root, isTabWithDescription("FirstTab")));
        onView(isTabWithDescription("FirstTab")).check(matches(isDisplayed()));
        onView(isTabWithDescription("SecondTab")).check(matches(isDisplayed()));
        onView(isTabWithDescription("ThirdTab")).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(() -> mModel.get(TABS).remove(mModel.get(TABS).get(1)));

        onView(isRoot()).check(
                (root, e)
                        -> waitForView((ViewGroup) root, isTabWithDescription("SecondTab"),
                                VIEW_INVISIBLE | VIEW_GONE | VIEW_NULL));
        onView(isTabWithDescription("FirstTab")).check(matches(isDisplayed()));
        onView(isTabWithDescription("SecondTab")).check(doesNotExist());
        onView(isTabWithDescription("ThirdTab")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAddsTabs() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(VISIBLE, true);
            mModel.get(TABS).set(new KeyboardAccessoryData.Tab[] {
                    createTestTab("FirstTab"), createTestTab("SecondTab")});
        });

        onView(isRoot()).check(
                (root, e) -> waitForView((ViewGroup) root, isTabWithDescription("FirstTab")));
        onView(isTabWithDescription("FirstTab")).check(matches(isDisplayed()));
        onView(isTabWithDescription("SecondTab")).check(matches(isDisplayed()));
        onView(isTabWithDescription("ThirdTab")).check(doesNotExist());

        ThreadUtils.runOnUiThreadBlocking(() -> mModel.get(TABS).add(createTestTab("ThirdTab")));

        onView(isRoot()).check(
                (root, e) -> waitForView((ViewGroup) root, isTabWithDescription("ThirdTab")));
        onView(isTabWithDescription("FirstTab")).check(matches(isDisplayed()));
        onView(isTabWithDescription("SecondTab")).check(matches(isDisplayed()));
        onView(isTabWithDescription("ThirdTab")).check(matches(isDisplayed()));
    }
}