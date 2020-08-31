// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.longClick;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import android.support.test.filters.SmallTest;
import android.view.ContextMenu;
import android.view.MenuItem;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 * Test related to {@link HomeButton}.
 *
 * Currently the change only affects when {@link ChromeFeatureList#HOMEPAGE_SETTINGS_UI_CONVERSION}
 * is enabled.
 * TODO: Add more test when features related has update.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class HomeButtonTest extends DummyUiActivityTestCase {
    private static final String ASSERT_MSG_MENU_IS_CREATED =
            "ContextMenu is not created after long press.";
    private static final String ASSERT_MSG_MENU_SIZE =
            "ContextMenu has a different size than test setting.";
    private static final String ASSERT_MSG_MENU_ITEM_TEXT =
            "MenuItem does shows different text than test setting.";

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    @Mock
    private SettingsLauncher mSettingsLauncher;

    private HomeButton mHomeButton;
    private int mIdHomeButton;


    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        MockitoAnnotations.initMocks(this);

        // Set the default test status for homepage button tests.
        // By default, the homepage is <b>enabled</b> and with customized URL.
        mHomepageTestRule.useCustomizedHomepageForTest("https://www.chromium.org/");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FrameLayout content = new FrameLayout(getActivity());
            getActivity().setContentView(content);

            mIdHomeButton = View.generateViewId();
            mHomeButton = new HomeButton(getActivity(), null);
            mHomeButton.setId(mIdHomeButton);
            mHomeButton.setSettingsLauncherForTests(mSettingsLauncher);
            HomeButton.setSaveContextMenuForTests(true);

            content.addView(mHomeButton);
        });
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.HOMEPAGE_SETTINGS_UI_CONVERSION)
    public void testContextMenu_BeforeConversion() {
        onView(withId(mIdHomeButton)).perform(longClick());

        ContextMenu menu = mHomeButton.getMenuForTests();
        Assert.assertNotNull(ASSERT_MSG_MENU_IS_CREATED, menu);
        Assert.assertEquals(ASSERT_MSG_MENU_SIZE, 1, menu.size());

        MenuItem item_remove = menu.findItem(HomeButton.ID_REMOVE);
        Assert.assertNotNull("MenuItem 'Remove' is not added to menu", item_remove);
        Assert.assertEquals(ASSERT_MSG_MENU_ITEM_TEXT, item_remove.getTitle().toString(),
                getActivity().getResources().getString(R.string.remove));

        // Test click on context menu item
        Assert.assertTrue("Homepage should be enabled before clicking the menu item.",
                HomepageManager.isHomepageEnabled());
        onView(withText(R.string.remove)).perform(click());
        Assert.assertFalse("Homepage should be disabled after clicking the menu item.",
                HomepageManager.isHomepageEnabled());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.HOMEPAGE_SETTINGS_UI_CONVERSION)
    public void testContextMenu_AfterConversion() {
        onView(withId(mIdHomeButton)).perform(longClick());

        ContextMenu menu = mHomeButton.getMenuForTests();
        Assert.assertNotNull(ASSERT_MSG_MENU_IS_CREATED, menu);
        Assert.assertEquals(ASSERT_MSG_MENU_SIZE, 1, menu.size());

        MenuItem item_settings = menu.findItem(HomeButton.ID_SETTINGS);
        Assert.assertNotNull("MenuItem 'Edit Homepage' is not added to menu", item_settings);
        Assert.assertEquals(ASSERT_MSG_MENU_ITEM_TEXT, item_settings.getTitle().toString(),
                getActivity().getResources().getString(R.string.options_homepage_edit_title));

        // Test click on context menu item
        onView(withText(R.string.options_homepage_edit_title)).perform(click());
        Mockito.verify(mSettingsLauncher)
                .launchSettingsActivity(mHomeButton.getContext(), HomepageSettings.class);
    }
}
