// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertNull;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.support.test.filters.MediumTest;
import android.support.v4.view.ViewPager;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Tab;
import org.chromium.chrome.browser.modelutil.LazyConstructionPropertyMcp;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.DeferredViewStubInflationProvider;
import org.chromium.ui.ViewProvider;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ExecutionException;

/**
 * View tests for the keyboard accessory sheet component.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccessorySheetViewTest {
    private AccessorySheetModel mModel;
    private BlockingQueue<ViewPager> mViewPager;

    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ViewStub viewStub = mActivityTestRule.getActivity().findViewById(
                    R.id.keyboard_accessory_sheet_stub);
            mModel = new AccessorySheetModel();
            mModel.setHeight(mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                    org.chromium.chrome.R.dimen.keyboard_accessory_sheet_height));
            ViewProvider<ViewPager> provider = new DeferredViewStubInflationProvider<>(viewStub);
            mViewPager = new ArrayBlockingQueue<>(1);
            new LazyConstructionPropertyMcp<>(mModel, AccessorySheetModel.PropertyKey.VISIBLE,
                    AccessorySheetModel::isVisible, provider, AccessorySheetViewBinder::bind);
            provider.whenLoaded(mViewPager::add);
        });
    }

    @Test
    @MediumTest
    public void testAccessoryVisibilityChangedByModel()
            throws ExecutionException, InterruptedException {
        // Initially, there shouldn't be a view yet.
        assertNull(mViewPager.poll());

        // After setting the visibility to true, the view should exist and be visible.
        ThreadUtils.runOnUiThreadBlocking(() -> { mModel.setVisible(true); });
        ViewPager viewPager = mViewPager.take();
        assertEquals(viewPager.getVisibility(), View.VISIBLE);

        // After hiding the view, the view should still exist but be invisible.
        ThreadUtils.runOnUiThreadBlocking(() -> { mModel.setVisible(false); });
        assertNotEquals(viewPager.getVisibility(), View.VISIBLE);
    }

    @Test
    @MediumTest
    public void testAddingTabToModelRendersTabsView() throws InterruptedException {
        final String kSampleAction = "Some Action";
        mModel.getTabList().add(new Tab(null, null, R.layout.empty_accessory_sheet,
                AccessoryTabType.PASSWORDS, new Tab.Listener() {
                    @Override
                    public void onTabCreated(ViewGroup view) {
                        assertNotNull("The tab must have been created!", view);
                        assertTrue("Empty tab is a layout.", view instanceof LinearLayout);
                        LinearLayout baseLayout = (LinearLayout) view;
                        TextView sampleTextView = new TextView(mActivityTestRule.getActivity());
                        sampleTextView.setText(kSampleAction);
                        baseLayout.addView(sampleTextView);
                    }

                    @Override
                    public void onTabShown() {}
                }));
        mModel.setActiveTabIndex(0);
        // Shouldn't cause the view to be inflated.
        assertNull(mViewPager.poll());

        // Setting visibility should cause the Tab to be rendered.
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.setVisible(true));
        assertNotNull(mViewPager.take());

        onView(withText(kSampleAction)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSettingActiveTabIndexChangesTab() {
        final String kFirstTab = "First Tab";
        final String kSecondTab = "Second Tab";
        mModel.getTabList().add(createTestTabWithTextView(kFirstTab));
        mModel.getTabList().add(createTestTabWithTextView(kSecondTab));
        mModel.setActiveTabIndex(0);
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.setVisible(true)); // Render view.

        onView(withText(kFirstTab)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(() -> mModel.setActiveTabIndex(1));

        onView(isRoot()).check((r, e) -> waitForView((ViewGroup) r, withText(kSecondTab)));
    }

    @Test
    @MediumTest
    public void testRemovingTabDeletesItsView() {
        final String kFirstTab = "First Tab";
        final String kSecondTab = "Second Tab";
        mModel.getTabList().add(createTestTabWithTextView(kFirstTab));
        mModel.getTabList().add(createTestTabWithTextView(kSecondTab));
        mModel.setActiveTabIndex(0);
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.setVisible(true)); // Render view.

        onView(withText(kFirstTab)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.getTabList().remove(mModel.getTabList().get(0)));

        onView(withText(kFirstTab)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testReplaceLastTab() {
        final String kFirstTab = "First Tab";
        mModel.getTabList().add(createTestTabWithTextView(kFirstTab));
        mModel.setActiveTabIndex(0);
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.setVisible(true)); // Render view.

        // Remove the last tab.
        onView(withText(kFirstTab)).check(matches(isDisplayed()));
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mModel.getTabList().remove(mModel.getTabList().get(0)); });
        onView(withText(kFirstTab)).check(doesNotExist());

        // Add a new first tab.
        final String kSecondTab = "Second Tab";
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.getTabList().add(createTestTabWithTextView(kSecondTab));
            mModel.setActiveTabIndex(0);
        });
        onView(isRoot()).check((r, e) -> waitForView((ViewGroup) r, withText(kSecondTab)));
    }

    private Tab createTestTabWithTextView(String textViewCaption) {
        return new Tab(null, null, R.layout.empty_accessory_sheet, AccessoryTabType.PASSWORDS,
                new Tab.Listener() {
                    @Override
                    public void onTabCreated(ViewGroup view) {
                        TextView sampleTextView = new TextView(mActivityTestRule.getActivity());
                        sampleTextView.setText(textViewCaption);
                        view.addView(sampleTextView);
                    }

                    @Override
                    public void onTabShown() {}
                });
    }
}