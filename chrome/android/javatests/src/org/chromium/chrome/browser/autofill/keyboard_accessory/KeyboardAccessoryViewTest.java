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
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertNull;

import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_INVISIBLE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryViewBinder.ActionViewBinder;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.modelutil.RecyclerViewModelChangeProcessor;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.atomic.AtomicReference;

/**
 * View tests for the keyboard accessory component.
 *
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class KeyboardAccessoryViewTest {
    private KeyboardAccessoryModel mModel;
    private KeyboardAccessoryViewBinder.AccessoryViewHolder mViewHolder;

    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    private static KeyboardAccessoryData.Action createTestAction(
            String caption, KeyboardAccessoryData.Action.Delegate delegate) {
        return new KeyboardAccessoryData.Action() {
            @Override
            public String getCaption() {
                return caption;
            }

            @Override
            public Delegate getDelegate() {
                return delegate;
            }
        };
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mModel = new KeyboardAccessoryModel();

        RecyclerViewAdapter<
                KeyboardAccessoryModel.SimpleListObservable<KeyboardAccessoryData.Action>,
                ActionViewBinder.ViewHolder> actionsAdapter =
                new RecyclerViewAdapter<>(mModel.getActionList(), new ActionViewBinder());
        mViewHolder = new KeyboardAccessoryViewBinder.AccessoryViewHolder(
                mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory_stub),
                actionsAdapter);
        mModel.addObserver(new PropertyModelChangeProcessor<>(
                mModel, mViewHolder, new KeyboardAccessoryViewBinder()));

        mModel.addActionListObserver(new RecyclerViewModelChangeProcessor<>(actionsAdapter));
    }

    @Test
    @MediumTest
    public void testAccessoryVisibilityChangedByModel() {
        // Initially, there shouldn't be a view yet.
        assertNull(mViewHolder.getView());

        // After setting the visibility to true, the view should exist and be visible.
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.setVisible(true));
        assertNotNull(mViewHolder.getView());
        assertTrue(mViewHolder.getView().getVisibility() == View.VISIBLE);

        // After hiding the view, the view should still exist but be invisible.
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.setVisible(false));
        assertNotNull(mViewHolder.getView());
        assertTrue(mViewHolder.getView().getVisibility() != View.VISIBLE);
    }

    @Test
    @MediumTest
    public void testClickableActionAddedWhenChangingModel() {
        final AtomicReference<Boolean> buttonClicked = new AtomicReference<>();
        final KeyboardAccessoryData.Action testAction =
                createTestAction("Test Button", action -> buttonClicked.set(true));

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.setVisible(true);
            mModel.getActionList().add(testAction);
        });

        onView(isRoot()).check((root, e) -> waitForView((ViewGroup) root, withText("Test Button")));
        onView(withText("Test Button")).perform(click());

        assertTrue(buttonClicked.get());
    }

    @Test
    @MediumTest
    public void testCanAddSingleButtons() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.setVisible(true);
            mModel.getActionList().set(
                    new KeyboardAccessoryData.Action[] {createTestAction("First", action -> {}),
                            createTestAction("Second", action -> {})});
        });

        onView(isRoot()).check((root, e) -> waitForView((ViewGroup) root, withText("First")));
        onView(withText("First")).check(matches(isDisplayed()));
        onView(withText("Second")).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.setVisible(true);
            mModel.getActionList().add(createTestAction("Third", action -> {}));
        });

        onView(isRoot()).check((root, e) -> waitForView((ViewGroup) root, withText("Third")));
        onView(withText("First")).check(matches(isDisplayed()));
        onView(withText("Second")).check(matches(isDisplayed()));
        onView(withText("Third")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testCanRemoveSingleButtons() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.setVisible(true);
            mModel.getActionList().set(
                    new KeyboardAccessoryData.Action[] {createTestAction("First", action -> {}),
                            createTestAction("Second", action -> {}),
                            createTestAction("Third", action -> {})});
        });

        onView(isRoot()).check((root, e) -> waitForView((ViewGroup) root, withText("First")));
        onView(withText("First")).check(matches(isDisplayed()));
        onView(withText("Second")).check(matches(isDisplayed()));
        onView(withText("Third")).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.getActionList().remove(mModel.getActionList().get(1)));

        onView(isRoot()).check((root, e)
                                       -> waitForView((ViewGroup) root, withText("Second"),
                                               VIEW_INVISIBLE | VIEW_GONE | VIEW_NULL));
        onView(withText("First")).check(matches(isDisplayed()));
        onView(withText("Second")).check(doesNotExist());
        onView(withText("Third")).check(matches(isDisplayed()));
    }
}