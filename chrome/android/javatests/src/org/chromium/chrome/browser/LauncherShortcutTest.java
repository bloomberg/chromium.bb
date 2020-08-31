// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.TargetApi;
import android.content.Intent;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests for Android NMR1 launcher shortcuts.
 */
// clang-format off
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@TargetApi(VERSION_CODES.N_MR1)
@MinAndroidSdkLevel(Build.VERSION_CODES.N_MR1)
public class LauncherShortcutTest {
    // clang-format on

    /**
     * Used for parameterized tests to toggle whether an incognito or regular tab is created.
     */
    public static class IncognitoParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(new ParameterSet().value(false).name("RegularTab"),
                    new ParameterSet().value(true).name("IncognitoTab"));
        }
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private TabModelSelector mTabModelSelector;
    private CallbackHelper mTabAddedCallback = new CallbackHelper();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelector();

        TabModelSelectorObserver tabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
                mTabAddedCallback.notifyCalled();
            }
        };
        mTabModelSelector.addObserver(tabModelSelectorObserver);
    }

    @After
    public void tearDown() {
        LauncherShortcutActivity.setDynamicShortcutStringForTesting(null);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(IncognitoParams.class)
    public void testLauncherShortcut(boolean incognito) throws Exception {
        int initialTabCount = mTabModelSelector.getTotalTabCount();

        Intent intent =
                new Intent(incognito ? LauncherShortcutActivity.ACTION_OPEN_NEW_INCOGNITO_TAB
                                     : LauncherShortcutActivity.ACTION_OPEN_NEW_TAB);
        intent.setClass(mActivityTestRule.getActivity(), LauncherShortcutActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);

        mTabAddedCallback.waitForCallback(0);

        // Verify NTP was created.

        Tab activityTab = TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getActivityTab());
        Assert.assertEquals("Incorrect tab launch type.", TabLaunchType.FROM_LAUNCHER_SHORTCUT,
                activityTab.getLaunchType());
        Assert.assertTrue("Tab should be an NTP. Tab url: " + activityTab.getUrl(),
                NewTabPage.isNTPUrl(activityTab.getUrl()));

        // Verify tab model.
        Assert.assertEquals("Incorrect tab model selected.", incognito,
                mTabModelSelector.isIncognitoSelected());
        Assert.assertEquals("Incorrect total tab count.", initialTabCount + 1,
                mTabModelSelector.getTotalTabCount());
        Assert.assertEquals("Incorrect normal tab count.",
                incognito ? initialTabCount : initialTabCount + 1,
                mTabModelSelector.getModel(false).getCount());
        Assert.assertEquals("Incorrect incognito tab count.", incognito ? 1 : 0,
                mTabModelSelector.getModel(true).getCount());
    }

    @Test(expected = TimeoutException.class)
    @MediumTest
    public void testInvalidIntent() throws TimeoutException {
        Intent intent = new Intent("fooAction");
        intent.setClass(mActivityTestRule.getActivity(), LauncherShortcutActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);

        mTabAddedCallback.waitForCallback(0);
    }

    @Test
    @MediumTest
    public void testManifestShortcuts() {
        ShortcutManager shortcutManager =
                mActivityTestRule.getActivity().getSystemService(ShortcutManager.class);
        List<ShortcutInfo> shortcuts = shortcutManager.getManifestShortcuts();
        Assert.assertEquals("Incorrect number of manifest shortcuts.", 1, shortcuts.size());
        Assert.assertEquals(
                "Incorrect manifest shortcut id.", "new-tab-shortcut", shortcuts.get(0).getId());
    }

    @Test
    @SmallTest
    public void testDynamicShortcuts() {
        IncognitoUtils.setEnabledForTesting(true);
        LauncherShortcutActivity.updateIncognitoShortcut(mActivityTestRule.getActivity());
        ShortcutManager shortcutManager =
                mActivityTestRule.getActivity().getSystemService(ShortcutManager.class);
        List<ShortcutInfo> shortcuts = shortcutManager.getDynamicShortcuts();
        Assert.assertEquals("Incorrect number of dynamic shortcuts.", 1, shortcuts.size());
        Assert.assertEquals("Incorrect dynamic shortcut id.",
                LauncherShortcutActivity.DYNAMIC_OPEN_NEW_INCOGNITO_TAB_ID,
                shortcuts.get(0).getId());

        IncognitoUtils.setEnabledForTesting(false);
        LauncherShortcutActivity.updateIncognitoShortcut(mActivityTestRule.getActivity());
        shortcuts = shortcutManager.getDynamicShortcuts();
        Assert.assertEquals("Incorrect number of dynamic shortcuts.", 0, shortcuts.size());

        IncognitoUtils.setEnabledForTesting(true);
        LauncherShortcutActivity.updateIncognitoShortcut(mActivityTestRule.getActivity());
        shortcuts = shortcutManager.getDynamicShortcuts();
        Assert.assertEquals("Incorrect number of dynamic shortcuts after re-enabling incognito.", 1,
                shortcuts.size());
    }

    @Test
    @SmallTest
    public void testDynamicShortcuts_LanguageChange() {
        IncognitoUtils.setEnabledForTesting(true);
        LauncherShortcutActivity.updateIncognitoShortcut(mActivityTestRule.getActivity());
        ShortcutManager shortcutManager =
                mActivityTestRule.getActivity().getSystemService(ShortcutManager.class);
        List<ShortcutInfo> shortcuts = shortcutManager.getDynamicShortcuts();
        Assert.assertEquals("Incorrect number of dynamic shortcuts.", 1, shortcuts.size());
        Assert.assertEquals(
                "Incorrect label", "New incognito tab", shortcuts.get(0).getLongLabel());

        LauncherShortcutActivity.setDynamicShortcutStringForTesting("Foo");
        LauncherShortcutActivity.updateIncognitoShortcut(mActivityTestRule.getActivity());
        shortcuts = shortcutManager.getDynamicShortcuts();
        Assert.assertEquals(
                "Incorrect number of dynamic shortcuts after updating.", 1, shortcuts.size());
        Assert.assertEquals(
                "Incorrect label after updating.", "Foo", shortcuts.get(0).getLongLabel());
    }
}
