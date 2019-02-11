// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.core.AllOf.allOf;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryTabLayoutProperties.ACTIVE_TAB;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryTabLayoutProperties.TABS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryTabLayoutProperties.TAB_SELECTION_CALLBACKS;
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
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View tests for the keyboard accessory tab layout component.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class KeyboardAccessoryTabLayoutViewTest {
    private PropertyModel mModel;
    private KeyboardAccessoryTabLayoutView mView;

    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    private KeyboardAccessoryData.Tab createTestTab(String contentDescription) {
        return new KeyboardAccessoryData.Tab("Passwords",
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
            mModel = new PropertyModel.Builder(TABS, ACTIVE_TAB, TAB_SELECTION_CALLBACKS)
                             .with(TABS, new ListModel<>())
                             .with(ACTIVE_TAB, null)
                             .build();
            ViewStub viewStub =
                    mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory_stub);
            mView = viewStub.inflate().findViewById(R.id.tabs);
            KeyboardAccessoryTabLayoutCoordinator.createTabViewBinder(mModel, mView);
        });
    }

    @Test
    @MediumTest
    public void testRemovesTabs() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ((KeyboardAccessoryView) mView.getParent().getParent()).setVisible(true);
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
            ((KeyboardAccessoryView) mView.getParent().getParent()).setVisible(true);
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